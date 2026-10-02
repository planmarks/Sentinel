/* Muru Vault - user interface: menus, on-screen QWERTY keyboard, screens,
 * idle auto-sleep and the top-level application loop. */
#include "board.h"
#include "ssd1306.h"
#include "buttons.h"
#include "vault.h"
#include "bip39.h"
#include "diceware.h"
#include "rng.h"
#include "sha256.h"
#include "lowpower.h"
#include "qrcodegen.h"
#include "ui.h"
#include <string.h>
#include <setjmp.h>

#define IDLE_MS       120000u          /* 2 minutes -> low power */
#define MAXPASS       160              /* passkey / passphrase buffer */
#define SUPPORT_URL   "https://muru.global/support"
#define WEBSITE_URL   "https://muru.global"

static jmp_buf g_home;                  /* longjmp target: re-lock + home */
static bool    g_session = false;       /* unlocked session active */
static bool    g_vault_authed = false;  /* passed the 3-word guard this session */

static bool guard_verify_entry(void);   /* defined below; used by recovery */

/* ---------- low-level draw helpers ---------- */
static void header(const char *title) {
  fb_fillrect(0, 0, OLED_W, 11, true);
  fb_text(2, 2, title, 1, false);
}

static void footer(const char *s) { fb_text(0, 57, s, 1, true); }

static void screen_begin(const char *title) { fb_clear(); header(title); }
static void screen_end(void) { oled_flush(); }

/* Big centered two/three-line message; waits for a key. */
static void message(const char *l1, const char *l2, const char *l3) {
  fb_clear();
  if (l1) fb_text((OLED_W - text_width(l1,1))/2, 16, l1, 1, true);
  if (l2) fb_text((OLED_W - text_width(l2,1))/2, 30, l2, 1, true);
  if (l3) fb_text((OLED_W - text_width(l3,1))/2, 44, l3, 1, true);
  oled_flush();
}

/* ---------- animated "busy" feedback ---------- */
static const char *g_busy_title = "Working";
static int         s_busy_seg   = -1;

/* Draw a progress bar + percent + spinner at `seg` of 32. */
static void busy_draw(int seg) {
  if (seg < 0) seg = 0;
  if (seg > 32) seg = 32;
  fb_clear();
  header(g_busy_title);
  const int x = 10, y = 28, w = 108, h = 14;
  fb_rect(x, y, w, h, true);
  fb_fillrect(x + 2, y + 2, (w - 4) * seg / 32, h - 4, true);
  int pct = seg * 100 / 32;
  char p[6]; int n = 0;
  if (pct >= 100) { p[n++]='1'; p[n++]='0'; p[n++]='0'; }
  else { if (pct >= 10) p[n++] = (char)('0' + pct/10); p[n++] = (char)('0' + pct%10); }
  p[n++] = '%'; p[n] = 0;
  fb_text((OLED_W - text_width(p,1))/2, 48, p, 1, true);
  char sp[2] = { "|/-\\"[seg & 3], 0 };
  fb_text(118, 2, sp, 1, false);   /* spinner glyph in the header bar */
  oled_flush();
}

/* Progress callback for pbkdf2: throttles redraws to whole segments. */
static void busy_progress(uint32_t done, uint32_t total) {
  int seg = total ? (int)((uint64_t)done * 32 / total) : 0;
  if (seg == s_busy_seg) return;
  s_busy_seg = seg;
  busy_draw(seg);
}

/* Wrap a blocking PBKDF2 call: busy_begin(title) ... op ... busy_end(). */
static void busy_begin(const char *title) {
  g_busy_title = title; s_busy_seg = -1; busy_draw(0);
  pbkdf2_set_progress(busy_progress);
}
static void busy_end(void) { pbkdf2_set_progress(0); }

/* Single instant frame for short operations (flash writes, generation). */
static void busy_flash(const char *title) { g_busy_title = title; busy_draw(0); }

/* ---------- idle-aware event read ---------- */
static void go_sleep(void) {
  oled_off();
  vault_lock();
  g_session = false;
  low_power_enter();      /* returns after OK wakes the CPU */
  oled_init();
  longjmp(g_home, 1);     /* unwind to the home/unlock flow */
}

static int ui_event(void) {
  int ev = read_event();
  if (ev == EV_NONE &&
      (uint32_t)(millis() - last_activity_ms()) > IDLE_MS) {
    go_sleep();   /* applies whether locked or unlocked */
  }
  return ev;
}

static void wait_key(void) { while (ui_event() == EV_NONE) {} }

/* ---------- scrolling list menu ---------- */
static int menu(const char *title, const char *const *items, int count) {
  int sel = 0, top = 0;
  const int rows = 5;
  for (;;) {
    screen_begin(title);
    for (int r = 0; r < rows && (top + r) < count; r++) {
      int idx = top + r, y = 14 + r * 10;
      if (idx == sel) { fb_fillrect(0, y - 1, OLED_W, 10, true); fb_text(2, y, items[idx], 1, false); }
      else fb_text(2, y, items[idx], 1, true);
    }
    if (top > 0)            fb_text(122, 13, "^", 1, true);
    if (top + rows < count) fb_text(122, 54, "v", 1, true);
    screen_end();

    int ev = ui_event();
    if (ev == EV_UP   && sel > 0)          sel--;
    if (ev == EV_DOWN && sel < count - 1)  sel++;
    if (ev == EV_OK)   return sel;
    if (ev == EV_BACK) return -1;
    if (sel < top) top = sel;
    if (sel >= top + rows) top = sel - rows + 1;
  }
}

/* ---------- on-screen QWERTY keyboard ---------- */
static const char *KB_LETTERS[3] = { "qwertyuiop", "asdfghjkl", "zxcvbnm" };
static const char *KB_SYMBOLS[3] = { "1234567890", "!@#$%^&*()", "-_=+.,?/:;" };

static int kb_row_len(int row, int layer) {
  if (row < 3) return (int)strlen(layer ? KB_SYMBOLS[row] : KB_LETTERS[row]);
  return 4;   /* function row */
}

/* Returns true if confirmed (long OK), false if cancelled (long BACK). */
static bool get_text(const char *title, char *out, int maxlen, bool masked) {
  const int ROWS = 4, kb_top = 30, rowH = 9;
  bool caps = false; int layer = 0, r = 1, c = 0;
  char buf[160]; int len = 0; buf[0] = 0;   /* fits a full 12-word phrase */
  if (maxlen > (int)sizeof(buf)) maxlen = sizeof(buf);

  for (;;) {
    int rlen = kb_row_len(r, layer);
    if (c >= rlen) c = rlen - 1;
    if (c < 0) c = 0;

    fb_clear();
    header(title);
    /* typed text, big (scale 2), tail-visible */
    char shown[16];
    int n = len; const char *sp = buf;
    const int maxc = 10;   /* scale-2 chars that fit across 128 px */
    if (masked) { int m=(n<maxc)?n:maxc; for (int i=0;i<m;i++) shown[i]='*'; shown[m]=0; }
    else {
      int start = (n > maxc) ? n - maxc : 0;
      strncpy(shown, sp + start, maxc); shown[maxc] = 0;
    }
    if (n == 0) fb_text(2, 14, "[hold OK=save]", 1, true);
    else        fb_text(2, 13, shown, 2, true);

    /* letter/symbol rows */
    for (int row = 0; row < 3; row++) {
      const char *rs = layer ? KB_SYMBOLS[row] : KB_LETTERS[row];
      int l = strlen(rs), cellW = OLED_W / l, y = kb_top + row * rowH;
      for (int k = 0; k < l; k++) {
        int x = k * cellW; char ch = rs[k];
        if (!layer && caps && ch >= 'a' && ch <= 'z') ch -= 32;
        bool selk = (row == r && k == c);
        if (selk) fb_fillrect(x, y - 1, cellW, rowH, true);
        char s2[2] = { ch, 0 };
        fb_text(x + (cellW - 6)/2, y, s2, 1, !selk);
      }
    }
    /* function row */
    {
      int y = kb_top + 3 * rowH, cellW = OLED_W / 4;
      const char *labels[4] = { caps ? "CAP" : "cap", layer ? "abc" : "123", "spc", "del" };
      for (int k = 0; k < 4; k++) {
        int x = k * cellW; bool selk = (r == 3 && k == c);
        if (selk) fb_fillrect(x, y - 1, cellW, rowH, true);
        int lw = strlen(labels[k]) * 6;
        fb_text(x + (cellW - lw)/2, y, labels[k], 1, !selk);
      }
    }
    screen_end();

    int ev = ui_event();
    if (ev == EV_UP)    r = (r + ROWS - 1) % ROWS;
    if (ev == EV_DOWN)  r = (r + 1) % ROWS;
    if (ev == EV_LEFT)  { int L = kb_row_len(r, layer); c = (c + L - 1) % L; }
    if (ev == EV_RIGHT) { int L = kb_row_len(r, layer); c = (c + 1) % L; }
    if (ev == EV_BACK)  { if (len > 0) buf[--len] = 0; }
    if (ev == EV_OK_LONG)   { strncpy(out, buf, maxlen - 1); out[maxlen-1] = 0; return true; }
    if (ev == EV_BACK_LONG) { out[0] = 0; return false; }
    if (ev == EV_OK) {
      if (r < 3) {
        const char *rs = layer ? KB_SYMBOLS[r] : KB_LETTERS[r];
        char ch = rs[c];
        if (!layer && caps && ch >= 'a' && ch <= 'z') ch -= 32;
        if (len < maxlen - 1) { buf[len++] = ch; buf[len] = 0; }
      } else {
        switch (c) {
          case 0: caps = !caps; break;
          case 1: layer ^= 1; break;
          case 2: if (len < maxlen - 1) { buf[len++] = ' '; buf[len] = 0; } break;
          case 3: if (len > 0) buf[--len] = 0; break;
        }
      }
    }
  }
}

/* ---------- passkey entry (masked keyboard, min length) ---------- */
static bool enter_passkey(const char *title, char *out, int maxlen) {
  for (;;) {
    if (!get_text(title, out, maxlen, true)) return false;
    if ((int)strlen(out) >= 4) return true;
    message("Passkey too short", "min 4 characters", 0); delay_ms(900); wait_key();
  }
}

/* ---------- wrapped text viewer ---------- */
static void draw_wrapped(int x, int y, const char *s) {
  const int cw = 6, maxc = (OLED_W - x) / cw;
  int col = 0, cy = y;
  for (const char *p = s; *p; p++) {
    if (col >= maxc) { col = 0; cy += 9; }
    char s2[2] = { *p, 0 };
    fb_text(x + col * cw, cy, s2, 1, true);
    col++;
  }
}

/* ================= screens ================= */

/* Show the 12-word recovery phrase across pages, force acknowledgement. */
static void show_seed(const char *phrase) {
  char words[12][10]; int nw = 0;
  const char *p = phrase;
  while (*p && nw < 12) {
    while (*p == ' ') p++;
    int k = 0; while (*p && *p != ' ' && k < 9) words[nw][k++] = *p++;
    words[nw][k] = 0; if (k) nw++;
  }
  int page = 0;                 /* 0..1, 6 words each */
  for (;;) {
    screen_begin("Recovery phrase");
    for (int i = 0; i < 6; i++) {
      int wi = page * 6 + i;
      if (wi >= nw) break;
      char line[20];
      int idx = wi + 1, li = 0;
      if (idx >= 10) line[li++] = '0' + idx/10;
      line[li++] = '0' + idx%10; line[li++] = '.'; line[li++] = ' ';
      strcpy(line + li, words[wi]);
      fb_text(4, 13 + i * 7, line, 1, true);
    }
    footer(page ? "LR:page  OK:done" : "LR:page  OK:next");
    screen_end();
    int ev = ui_event();
    if (ev == EV_LEFT || ev == EV_RIGHT) page ^= 1;
    if (ev == EV_OK) { if (page == 1) return; page = 1; }
  }
}

static void save_prompt(const char *pass) {
  Entry e; memset(&e, 0, sizeof(e));
  if (!get_text("Name / Label", e.label, LBL_LEN, false)) return;
  if (!get_text("Username", e.user, USR_LEN, false)) return;
  strncpy(e.pass, pass, PWD_LEN - 1);
  busy_flash("Saving");
  if (vault_add(&e)) message("Saved to vault", 0, 0);
  else               message("Vault full or", "save failed", 0);
  delay_ms(400); wait_key();
}

static void view_entry(int idx) {
  Entry e;
  if (!vault_get(idx, &e)) { message("Read error", 0, 0); wait_key(); return; }
  int page = 0;   /* 0 user, 1 pass */
  for (;;) {
    screen_begin(e.label[0] ? e.label : "(no label)");
    if (page == 0) { fb_text(0, 14, "User:", 1, true); draw_wrapped(0, 24, e.user); }
    else           { fb_text(0, 14, "Pass:", 1, true); draw_wrapped(0, 24, e.pass); }
    footer("LR:pg OK:del BK:x");
    screen_end();
    int ev = ui_event();
    if (ev == EV_LEFT || ev == EV_RIGHT) page ^= 1;
    if (ev == EV_BACK) return;
    if (ev == EV_OK) {
      screen_begin("Delete?");
      fb_text(0, 20, "Delete this entry?", 1, true);
      footer("OK:yes  BACK:no");
      screen_end();
      int c; do { c = ui_event(); } while (c != EV_OK && c != EV_BACK);
      if (c == EV_OK) { vault_delete(idx); message("Deleted", 0, 0); wait_key(); return; }
    }
  }
}

static void vault_browser(void) {
  for (;;) {
    int n = vault_count();
    if (n == 0) { message("Vault is empty", "Add a password", 0); wait_key(); return; }
    static char labels[VAULT_MAX_ENTRIES][LBL_LEN + 4];
    const char *items[VAULT_MAX_ENTRIES];
    for (int i = 0; i < n; i++) {
      Entry e;
      if (vault_get(i, &e) && e.label[0]) { strncpy(labels[i], e.label, LBL_LEN); labels[i][LBL_LEN]=0; }
      else { labels[i][0]='#'; labels[i][1]='0'+ (i+1)%10; labels[i][2]=0; }
      items[i] = labels[i];
    }
    int sel = menu("Vault", items, n);
    if (sel < 0) return;
    view_entry(sel);
  }
}

/* Generator config + generate, then offer to save. */
static void password_generate(void) {
  GenCfg cfg; gencfg_load(&cfg);
  int sel = 0;
  for (;;) {
    screen_begin("Generate");
    char b[6][22];
    int ln = cfg.length;
    b[0][0]='L';b[0][1]='e';b[0][2]='n';b[0][3]=':';b[0][4]=' ';
    { int li=5; if(ln>=10)b[0][li++]='0'+ln/10; b[0][li++]='0'+ln%10; b[0][li]=0; }
    #define ONOFF(v) ((v)?"ON":"off")
    strcpy(b[1],"abc: "); strcat(b[1],ONOFF(cfg.lower));
    strcpy(b[2],"ABC: "); strcat(b[2],ONOFF(cfg.upper));
    strcpy(b[3],"123: "); strcat(b[3],ONOFF(cfg.digits));
    strcpy(b[4],"sym: "); strcat(b[4],ONOFF(cfg.symbols));
    strcpy(b[5],">> GENERATE <<");
    for (int r = 0; r < 6; r++) {
      int y = 13 + r * 8;
      if (r == sel) { fb_fillrect(0, y-1, OLED_W, 8, true); fb_text(2, y, b[r], 1, false); }
      else fb_text(2, y, b[r], 1, true);
    }
    screen_end();
    int ev = ui_event();
    if (ev == EV_UP)   sel = (sel + 5) % 6;
    if (ev == EV_DOWN) sel = (sel + 1) % 6;
    if (ev == EV_BACK) { gencfg_save(&cfg); return; }
    if (ev == EV_LEFT || ev == EV_RIGHT) {
      int d = (ev == EV_RIGHT) ? 1 : -1;
      switch (sel) {
        case 0: cfg.length += d; if (cfg.length < 8) cfg.length = 8; if (cfg.length > PWD_LEN-1) cfg.length = PWD_LEN-1; break;
        case 1: cfg.lower ^= 1; break; case 2: cfg.upper ^= 1; break;
        case 3: cfg.digits ^= 1; break; case 4: cfg.symbols ^= 1; break;
      }
    }
    if (ev == EV_OK) {
      if (sel >= 1 && sel <= 4) {
        switch (sel) { case 1: cfg.lower^=1;break; case 2: cfg.upper^=1;break;
                       case 3: cfg.digits^=1;break; case 4: cfg.symbols^=1;break; }
      } else if (sel == 5) {
        char pw[PWD_LEN];
        if (vault_generate(&cfg, pw, PWD_LEN)) {
          gencfg_save(&cfg);
          for (;;) {
            screen_begin("Password");
            draw_wrapped(0, 14, pw);
            footer("OK:save  BACK:back");
            screen_end();
            int e2 = ui_event();
            if (e2 == EV_BACK) break;
            if (e2 == EV_OK) { save_prompt(pw); break; }
          }
          memset(pw, 0, sizeof(pw));
        } else { message("Enable at least", "one character set", 0); wait_key(); }
      }
    }
  }
}

static void password_manual(void) {
  char pw[PWD_LEN];
  if (!get_text("Type password", pw, PWD_LEN, false)) return;
  if (pw[0] == 0) return;
  save_prompt(pw);
  memset(pw, 0, sizeof(pw));
}

static void new_password_menu(void) {
  const char *opts[] = { "Generate", "Manual" };
  int m = menu("New Password", opts, 2);
  if (m == 0) password_generate();
  else if (m == 1) password_manual();
}

/* Generate an Estonian Diceware passphrase: pick a word count, review, hold OK
 * to accept. The phrase itself (lowercase, space-separated) becomes the passkey
 * and runs through the same PBKDF2 -> key-wrap path as a typed passkey. */
static bool passphrase_generate(char *out, int outsz) {
  int nwords = 6;
  for (;;) {
    screen_begin("Estonian phrase");
    char l[20]; strcpy(l, "Words: "); l[7] = (char)('0' + nwords); l[8] = 0;
    fb_text(4, 16, l, 2, true);
    int bits = nwords * 11;
    char e[24]; int bi = 0;
    const char *pre = "approx "; while (pre[bi]) { e[bi] = pre[bi]; bi++; }
    if (bits >= 100) e[bi++] = (char)('0' + bits/100);
    e[bi++] = (char)('0' + (bits/10)%10); e[bi++] = (char)('0' + bits%10);
    strcpy(e + bi, " bits");
    fb_text(4, 36, e, 1, true);
    footer("LR:count OK:make BK:x");
    screen_end();
    int ev = ui_event();
    if (ev == EV_LEFT  && nwords > 4) nwords--;
    if (ev == EV_RIGHT && nwords < 8) nwords++;
    if (ev == EV_BACK) return false;
    if (ev == EV_OK) {
      bool regen = true;
      while (regen) {
        diceware_generate(nwords, out, outsz);
        for (;;) {
          screen_begin("Write this down");
          const char *q = out; char tok[16];
          for (int idx = 0; idx < nwords && *q; idx++) {
            while (*q == ' ') q++;
            int k = 0; while (*q && *q != ' ' && k < 15) tok[k++] = *q++;
            tok[k] = 0;
            char line[20]; int li = 0;
            if (idx + 1 >= 10) line[li++] = (char)('0' + (idx+1)/10);
            line[li++] = (char)('0' + (idx+1)%10); line[li++] = '.'; line[li++] = ' ';
            strcpy(line + li, tok);
            fb_text(2, 13 + idx * 7, line, 1, true);
          }
          footer("holdOK:use OK:new");
          screen_end();
          int e2 = ui_event();
          if (e2 == EV_OK_LONG)   { return true; }
          if (e2 == EV_OK)        { break; }          /* regenerate */
          if (e2 == EV_BACK)      { regen = false; break; } /* back to count */
        }
      }
    }
  }
}

/* Offer "type a passkey" or "Estonian passphrase", then store it. */
static bool set_passkey_flow(void) {
  const char *opts[] = { "Type passkey", "Estonian phrase" };
  int m = menu("Set passkey", opts, 2);
  if (m < 0) return false;
  char pk[MAXPASS];
  int kind = (m == 1) ? 1 : 0;
  if (m == 1) {
    if (!passphrase_generate(pk, sizeof(pk))) return false;
    message("This phrase IS", "your passkey - type", "it to unlock"); wait_key();
  } else {
    char pk2[MAXPASS];
    if (!enter_passkey("Set passkey", pk, sizeof(pk))) return false;
    if (!enter_passkey("Repeat passkey", pk2, sizeof(pk2)) || strcmp(pk, pk2) != 0) {
      message("Passkeys differ", "try again", 0); wait_key();
      memset(pk, 0, sizeof(pk)); memset(pk2, 0, sizeof(pk2)); return false;
    }
    memset(pk2, 0, sizeof(pk2));
  }
  busy_begin("Securing key");
  vault_set_passkey(pk, kind);
  busy_end();
  vault_set_attempts(0);
  memset(pk, 0, sizeof(pk));
  return true;
}

/* Recover: enter 12 words on the keyboard, then set a new passkey. */
static void recover_flow(void) {
  if (vault_has_data() && vault_guard_set()) {
    if (!guard_verify_entry()) { message("Guard words wrong", 0, 0); wait_key(); return; }
  }
  char phrase[160];
  message("Enter 12 words", "space-separated", 0); delay_ms(500); wait_key();
  if (!get_text("Seed phrase", phrase, sizeof(phrase), false)) return;
  if (!vault_recover(phrase)) { message("Invalid phrase", "check the words", 0); wait_key(); memset(phrase,0,sizeof(phrase)); return; }
  memset(phrase, 0, sizeof(phrase));
  if (!set_passkey_flow()) return;
  message("Vault recovered", 0, 0); wait_key();
  g_session = true;
}

static void settings_menu(void) {
  const char *opts[] = { "Shut Down", "Wipe", "Recover" };
  for (;;) {
    int m = menu("Settings", opts, 3);
    if (m < 0) return;
    if (m == 0) { go_sleep(); }
    if (m == 1) {
      screen_begin("Wipe");
      fb_text(0, 14, "Reset device to", 1, true);
      fb_text(0, 22, "factory state.", 1, true);
      fb_text(0, 32, "Vault stays sealed;", 1, true);
      fb_text(0, 40, "restore via Recover", 1, true);
      footer("OK:wipe BACK:no");
      screen_end();
      int c; do { c = ui_event(); } while (c != EV_OK && c != EV_BACK);
      if (c == EV_OK) { vault_soft_reset(); message("Device reset", 0, 0); wait_key(); longjmp(g_home, 1); }
    }
    if (m == 2) recover_flow();
  }
}

static void about_screen(void) {
  int page = 0;
  const char *pages[][4] = {
    { "Muru Vault", "Offline password", "generator & vault", "on Sentinel V1B" },
    { "AES-256 vault,", "device TRNG,", "12-word recovery.", "Keys never leave." },
    { "UP/DN/LR: move", "OK: select", "hold OK: save", "hold BACK: cancel" },
  };
  const int NP = 3;
  for (;;) {
    screen_begin("About");
    for (int i = 0; i < 4; i++) fb_text(0, 15 + i * 9, pages[page][i], 1, true);
    footer("LR:page  BACK:x");
    screen_end();
    int ev = ui_event();
    if (ev == EV_RIGHT) page = (page + 1) % NP;
    if (ev == EV_LEFT)  page = (page + NP - 1) % NP;
    if (ev == EV_BACK) return;
  }
}

/* Draw a QR (dark modules on a lit background, with a quiet zone) centered. */
static void draw_qr(const char *text) {
  static uint8_t modules[256];
  QRCode qr;
  if (qrcode_initText(&qr, modules, 3, ECC_MEDIUM, text) < 0) {
    if (qrcode_initText(&qr, modules, 4, ECC_LOW, text) < 0) { message("QR error", 0, 0); return; }
  }
  int sz = qr.size, scale = 1, quiet = 2;
  int dim = (sz + 2 * quiet) * scale;
  int ox = (OLED_W - dim) / 2, oy = (OLED_H - dim) / 2 + 2;
  fb_fillrect(ox, oy, dim, dim, true);           /* lit background + quiet zone */
  for (int y = 0; y < sz; y++)
    for (int x = 0; x < sz; x++)
      if (qrcode_getModule(&qr, x, y))
        fb_fillrect(ox + (quiet + x)*scale, oy + (quiet + y)*scale, scale, scale, false);
}

static void help_screen(void) {
  int page = 0;
  for (;;) {
    fb_clear(); header("Help / Support");
    if (page == 0) { draw_qr(SUPPORT_URL); footer("LR: show link"); }
    else {
      fb_text(0, 18, "Scan or visit:", 1, true);
      draw_wrapped(0, 30, "muru.global/support");
      footer("LR: show QR");
    }
    oled_flush();
    int ev = ui_event();
    if (ev == EV_LEFT || ev == EV_RIGHT) page ^= 1;
    if (ev == EV_BACK) return;
  }
}

static void credits_screen(void) {
  int page = 0;
  for (;;) {
    fb_clear(); header("Credits");
    if (page == 0) {
      fb_text(0, 16, "Created by", 1, true);
      fb_text(0, 28, "@papathought", 2, true);
      footer("LR: website QR");
    } else {
      draw_qr(WEBSITE_URL);
      footer("muru.global");
    }
    oled_flush();
    int ev = ui_event();
    if (ev == EV_LEFT || ev == EV_RIGHT) page ^= 1;
    if (ev == EV_BACK) return;
  }
}

/* ---------- 3-word guard (second factor for the saved-passwords page) ---------- */

/* Show the 3 guard words at creation; hold OK to acknowledge. */
static void show_guard(const int idx[3]) {
  for (;;) {
    screen_begin("Guard words");
    fb_text(0, 14, "Write down, in order:", 1, true);
    for (int i = 0; i < 3; i++) {
      char line[20]; line[0] = (char)('1'+i); line[1] = '.'; line[2] = ' ';
      strcpy(line + 3, diceware_word(idx[i]));
      fb_text(8, 26 + i * 10, line, 1, true);   /* 26,36,46 -> clears footer */
    }
    footer("hold OK: done");
    screen_end();
    if (ui_event() == EV_OK_LONG) return;
  }
}

/* Recognition challenge: 3 rounds, each the correct guard word + 2 decoys.
 * All three must be picked correctly, in order. */
static bool guard_challenge(void) {
  int g[3]; vault_get_guard(g);
  for (int r = 0; r < 3; r++) {
    int correct = g[r];
    int opt[3], used[4]; int nu = 0;
    int pos = (int)rng_below(3);
    opt[pos] = correct; used[nu++] = correct;
    for (int k = 0; k < 3; k++) {
      if (k == pos) continue;
      int d;
      for (;;) {
        d = (int)rng_below(2048);
        bool dup = false;
        for (int j = 0; j < nu; j++) if (used[j] == d) dup = true;
        if (!dup) break;
      }
      opt[k] = d; used[nu++] = d;
    }
    char title[16]; strcpy(title, "Guard  /3"); title[6] = (char)('1'+r);
    const char *items[3] = { diceware_word(opt[0]), diceware_word(opt[1]), diceware_word(opt[2]) };
    int sel = menu(title, items, 3);
    if (sel < 0 || opt[sel] != correct) return false;
  }
  return true;
}

/* Recovery gate: type the 3 guard words, in order. */
static bool guard_verify_entry(void) {
  int g[3]; vault_get_guard(g);
  char buf[80];
  message("Enter 3 guard", "words in order", 0); delay_ms(400); wait_key();
  if (!get_text("Guard words", buf, sizeof(buf), false)) return false;
  char tok[4][16]; int n = 0;
  const char *p = buf;
  while (*p && n < 4) {
    while (*p == ' ') p++;
    if (!*p) break;
    int k = 0; while (*p && *p != ' ' && k < 15) tok[n][k++] = *p++;
    tok[n][k] = 0; n++;
  }
  if (n != 3) return false;
  for (int i = 0; i < 3; i++)
    if (strcmp(tok[i], diceware_word(g[i])) != 0) return false;
  return true;
}

/* ---------- main menu ---------- */
static void main_menu(void) {
  const char *opts[] = { "Vault", "New Password", "Settings", "About", "Help", "Credits" };
  int m = menu("Muru Vault", opts, 6);
  if (m < 0) return;
  switch (m) {
    case 0:
      if (vault_guard_set() && !g_vault_authed) {
        if (guard_challenge()) g_vault_authed = true;
        else { message("Access denied", 0, 0); wait_key(); break; }
      }
      vault_browser();
      break;
    case 1: new_password_menu(); break;
    case 2: settings_menu(); break;
    case 3: about_screen(); break;
    case 4: help_screen(); break;
    case 5: credits_screen(); break;
  }
}

/* ---------- boot / unlock flows ---------- */
static void splash(void) {
  fb_clear();
  fb_text((OLED_W - text_width("Muru Vault", 2))/2, 12, "Muru Vault", 2, true);
  fb_text((OLED_W - text_width("by @papathought", 1))/2, 36, "by @papathought", 1, true);
  fb_text((OLED_W - text_width("muru.global", 1))/2, 48, "muru.global", 1, true);
  oled_flush();
  delay_ms(1600);
}

static void first_setup(void) {
  const char *opts[] = { "Create new vault", "Recover from seed" };
  int m = menu("Welcome", opts, 2);
  if (m == 1) {
    if (vault_has_data() && vault_guard_set()) {
      if (!guard_verify_entry()) { message("Guard words wrong", 0, 0); wait_key(); return; }
    }
    char phrase[160];
    if (!get_text("Seed phrase", phrase, sizeof(phrase), false)) { memset(phrase,0,sizeof(phrase)); return; }
    if (!vault_recover(phrase)) { message("Invalid phrase", 0, 0); wait_key(); memset(phrase,0,sizeof(phrase)); return; }
    memset(phrase, 0, sizeof(phrase));
  } else {
    char phrase[160];
    message("Creating vault", "generating seed", 0);
    if (!vault_new_identity(phrase, sizeof(phrase))) { message("Init failed", 0, 0); wait_key(); return; }
    message("Write these 12", "words down. They", "restore your vault"); delay_ms(600); wait_key();
    show_seed(phrase);
    memset(phrase, 0, sizeof(phrase));

    /* Generate 3 distinct guard words (second factor for saved passwords). */
    int gi[3], used[3], nu = 0;
    for (int i = 0; i < 3; i++) {
      int d;
      for (;;) {
        d = (int)rng_below(2048);
        bool dup = false;
        for (int j = 0; j < nu; j++) if (used[j] == d) dup = true;
        if (!dup) break;
      }
      gi[i] = d; used[nu++] = d;
    }
    message("Plus 3 guard words", "to open your", "saved passwords"); delay_ms(600); wait_key();
    show_guard(gi);
    busy_flash("Saving");
    vault_set_guard(gi);
  }
  /* set passkey (typed or Estonian passphrase) - mandatory, so loop until set */
  while (!set_passkey_flow()) { /* user cancelled; a passkey is required */ }
  message("Setup complete", 0, "Press OK to start"); wait_key();
  g_session = true;
}

static void unlock_flow(void) {
  for (;;) {
    int used = vault_attempts();
    if (used >= 3) { vault_destroy_all(); message("Too many tries", "device wiped", 0); wait_key(); longjmp(g_home, 1); }
    char pk[MAXPASS];
    const char *prompt = (vault_passkey_kind() == 1) ? "Enter passphrase" : "Enter passkey";
    if (!enter_passkey(prompt, pk, sizeof(pk))) continue;
    vault_set_attempts(used + 1);           /* count BEFORE the check */
    busy_begin("Unlocking");
    bool ok = vault_unlock(pk);
    busy_end();
    memset(pk, 0, sizeof(pk));
    if (ok) { vault_set_attempts(0); g_session = true; return; }
    char l2[20]; int left = 3 - (used + 1);
    strcpy(l2, "tries left: ");
    l2[11] = '0' + (left < 0 ? 0 : left); l2[12] = 0;
    message("Wrong passkey", l2, 0);
    delay_ms(800); wait_key();
  }
}

void ui_run(void) {
  splash();
  setjmp(g_home);          /* sleep/wipe/lock unwind returns here */
  g_session = false;
  g_vault_authed = false;  /* re-challenge the guard after every re-lock */
  for (;;) {
    if (!vault_auth_exists()) first_setup();
    else                      unlock_flow();
    while (g_session) main_menu();
    /* g_session cleared (shouldn't normally happen without longjmp) -> re-lock */
  }
}
