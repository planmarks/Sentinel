#!/usr/bin/env python3
"""Produce the hand-pruned 2048-word Estonian Diceware list + C header.
Inputs (same dir as given on the command line):
  et_50k.txt       Estonian frequency list (OpenSubtitles 2018, hermitdave/FrequencyWords)
  first-names.txt  English first-name dataset (dominictarr/random-name)
Pipeline: lowercase -> Estonian letters only -> ASCII-fold -> length 4-8 ->
  drop c/q/w/x/y/z/f -> drop personal names (dataset) -> drop the manual blocklist
  of surnames/places/English artifacts -> drop profanity/slur stems -> first unique
  4-letter prefix wins -> take 2048 -> sort. Usage: python curate_estonian.py <dir> <projdir>
Always re-run tools/validate_wordlist.py on the result.
"""
import sys, os
SP, BASE = sys.argv[1], sys.argv[2]
FOLD = str.maketrans({'õ':'o','ä':'a','ö':'o','ü':'u','š':'s','ž':'z'})
ALLOWED = set("abcdefghijklmnopqrstuvwxyzõäöüšž"); FOREIGNISH = set("cqwxyzf")
def norm(w): return w.strip().lower().replace('\ufeff','').translate(FOLD)

names = set()
for line in open(os.path.join(SP,"first-names.txt"), encoding="utf-8", errors="ignore"):
    n = norm(line)
    if n.isascii() and n.isalpha(): names.add(n)

manual = set("""
aaron adam alan albert alison amanda amelia angela angie anna anne annie anton april argo arthur
andre angelo antonio apollo baker barnes batman bender bernard bruno damien davis donovan douglas
eugene garrett hannibal hiro hill johnson jonas joel juan julian kirk leon luther madison moore
mulder omar philip pete ralph randall robinson roman romeo ragnar silas steven tanner turner viktor
nigel martini martin marvin mason matt megan melanie melissa merlin miguel mike miles milo miranda mona
barbara bart bella beni benjamin bernie beth bill bonnie brad brandon bree brian bridget burt
dale damon dana daniel daphne dave david dean debbie dennis derek diana diego donald donna doug
eddie edgar eleanor elena elijah ellen elliot emma erik ernie ethan evan
gabe gabriel gaius gemma george gina gloria gordon graham greg
hank hannah hans harold heather helen hugh hugo ivan
jaime jake james jamie jane jason jean jenna jess jill joan john jonathan jordan joseph josh judith jules julia june
kaia karen karl katie keith kent kevin klaus lana lane laura leonard leslie lester levi liam lionel logan lois louis luke
maggie maria mark nate natalie nathan naomi nikita nikki noah nolan nora norman oliver otto
pablo paul peter phil phoebe pierre piper prue regina reese rita robbie robert robin roger ronnie rose rosie ross ruth
sami santa sarah sasha saul sean serena seth shane sharon sheldon simon sophie stan stephen steve stuart susan
tess thomas todd travis trevor vera isver orna morva nassu
bauer bishop bond burke dunham green grant hammond harper holmes jones luthor marshall monroe morgan morris
nelson palmer parker pope porter potter russell sheppard shepard simpson sloane smith stark stone vaughn drake duke hitler homer
aafrika miami moskva narvis narvi londonis londoni berliini houston seattle egiptuse korea trooja vietnami araabia
east street house band subtitle boss room look join hope bingo bravo braavo super stopp stop pluss
little love blue long point south grand alpha delta
jajah nonii nojah tsau okei halloo hallo hurraa eksole aamen homo jaah jessas jeerum
angeles arnold aria davidi elvis evans jamesi jerome johni johker joshua roland sergei sherman simmons stevie thor jedi rove phillip senjoor monsieur milord mister madam kuuba iraagis turgi that times abdul ahmed carlos victor allan aris earl ellis gilbert hong jasper julius kane neil russ silver junior miss 
""".split())

# profanity / slurs as STEM prefixes to also catch inflected forms
prof_stems = ["pers","sitt","sita","pask","pasa","vitt","munn","hoor","porno","pede","neeger",
              "litapoeg","molak","idik","idioo","kurat","kurad","raibe","raisk","tiss","peenis",
              "jobu","lits","libu","lurjus","munni","sitap","perv","lesb","tura","kuram","lirv","hoora"]
def is_profane(f):
    return any(f.startswith(s) for s in prof_stems)

exclude = names | manual
seen_word, seen_prefix, chosen = set(), set(), []
for line in open(os.path.join(SP,"et_50k.txt"), encoding="utf-8"):
    p = line.split()
    if len(p) < 2: continue
    w = p[0].strip().lower()
    if not w or any(c not in ALLOWED for c in w): continue
    f = w.translate(FOLD)
    if not (f.isascii() and f.isalpha()): continue
    if not (4 <= len(f) <= 8): continue
    if any(c in FOREIGNISH for c in f): continue
    if f in exclude or is_profane(f): continue
    if f in seen_word or f[:4] in seen_prefix: continue
    seen_word.add(f); seen_prefix.add(f[:4]); chosen.append(f)
    if len(chosen) == 2048: break
print("collected:", len(chosen))
if len(chosen)!=2048: sys.exit("not enough: %d"%len(chosen))
chosen.sort()
open(os.path.join(BASE,"build","estonian_2048.txt"),"w",newline="\n",encoding="ascii").write("\n".join(chosen)+"\n")
