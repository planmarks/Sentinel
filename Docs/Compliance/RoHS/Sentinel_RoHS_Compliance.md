# RoHS Compliance – Sentinel Wallet

| | |
|---|---|
| **Document** | RoHS Declaration of Conformity and Compliance File |
| **Product** | Sentinel Wallet |
| **Model / BOM reference** | `Sentinel Wallet` sheet, *Muru Product Compliance.xlsx* |
| **Hardware revision** | `V1C` |
| **Document revision** | 1 |
| **Date** | 2026-10-03 |

---

## 1. Product identification

| | |
|---|---|
| Product name | Sentinel Wallet |
| Product description | Battery-powered device built on an STM32U585 microcontroller with an Infineon OPTIGA Trust M secure element, OLED display, 7 user switches, USB-C, a Li-ion charger (MCP73833) and battery protection (DW01A + FS8205A) – hardware wallet.  |
| Manufacturer (legal entity) | `Muru Global OU` |
| Address | `Tondi 8a, Tallinn, 11313, Harju county, Estonia` |
| Contact | `Oliver-Gerd Vand, info@muru.global` |
| Proposed RoHS category (Annex I) | Category 3 – IT and telecommunications equipment  |
| Identification on product | `model name` |

## 2. Declaration of Conformity (RoHS)

> We, `Muru Global OU`, `Tondi 8a, Tallinn, 11313, Harju county, Estonia`, declare under our sole responsibility that the product
>
> **Sentinel Wallet** – hardware revision `V1C`
>
> to which this declaration relates is in conformity with **Directive 2011/65/EU** of the European Parliament and of the Council of 8 June 2011 on the restriction of the use of certain hazardous substances in electrical and electronic equipment (RoHS), as amended by **Commission Delegated Directive (EU) 2015/863** (RoHS 3).
>
> The product does not contain any of the restricted substances listed in Annex II above the maximum concentration values given below, except for applications exempted under Annex III or Annex IV of the Directive as documented in the technical file.
>
> The technical documentation was prepared in accordance with the harmonised standard **EN IEC 63000:2018** – *Technical documentation for the assessment of electrical and electronic products with respect to the restriction of hazardous substances*.
>
> Place and date of issue: `Tallinn`, `2026-10-03`
>
> Signed for and on behalf of `Muru Global OU`:
>
> Name: `[name]` &nbsp;&nbsp; Function: `[function]` &nbsp;&nbsp; Signature: ____________________

### Restricted substances (Annex II, as amended by (EU) 2015/863)

| Substance | Max. concentration (by weight in homogeneous material) |
|---|---|
| Lead (Pb) | 0.1 % (1000 ppm) |
| Mercury (Hg) | 0.1 % (1000 ppm) |
| Cadmium (Cd) | 0.01 % (100 ppm) |
| Hexavalent chromium (Cr VI) | 0.1 % (1000 ppm) |
| Polybrominated biphenyls (PBB) | 0.1 % (1000 ppm) |
| Polybrominated diphenyl ethers (PBDE) | 0.1 % (1000 ppm) |
| Bis(2-ethylhexyl) phthalate (DEHP) | 0.1 % (1000 ppm) |
| Butyl benzyl phthalate (BBP) | 0.1 % (1000 ppm) |
| Dibutyl phthalate (DBP) | 0.1 % (1000 ppm) |
| Diisobutyl phthalate (DIBP) | 0.1 % (1000 ppm) |

### Exemptions (Annex III)

No device-level exemption is claimed. Some supplier declarations may rely on Annex III exemptions. These are typically **7(c)-I**, lead in glass or ceramic of thick-film resistors and ceramic capacitors, and **6(c)**, lead in copper alloys of connectors. Each exemption a supplier uses must be recorded against the component in the technical file and checked for validity on the date the product is placed on the market.

## 3. Basis of the assessment

This assessment follows the EN IEC 63000 supplier-information approach:

1. The bill of materials (BOM) of the device was taken from *Muru Product Compliance.xlsx*, sheet `Sentinel Wallet`.
2. For each component, the RoHS status was taken from the distributor (DigiKey) listing for the part number linked in the BOM.
3. Where available, a manufacturer RoHS certificate of compliance, declaration or material declaration was linked as evidence.
4. No analytical (XRF / laboratory) testing has been performed on the assembled product.
5. Risk assessment: components from large manufacturers with published certificates are low risk. Components with no published document are medium risk: no-name or second-source semiconductors, display modules, and hobby breakout boards or modules. For medium-risk components, a signed supplier declaration or a third-party test report (IEC 62321 series) must be obtained before signing.

## 4. Bill of materials and compliance evidence

BOM lines: **29** · Components with a fixed quantity: **59** · Lines without a published RoHS document: **4**

| # | Ref. | Qty | Description | Manufacturer | Part number (as linked) | RoHS status | Evidence |
|---|---|---|---|---|---|---|---|
| 1 | U4 | 1 | STM32U585CIT6 | STMicroelectronics | STM32U585CIT6 | ROHS3 Compliant | [ST material declaration (IPC-1752)](https://www.st.com/resource/en/material_declaration/625b_482xxxu.pdf) |
| 2 | U5 | 1 | SLS32AIA010MLUSON10XTMC1 | Infineon | SLS32AIA010MLUSON10XTMC1 (OPTIGA Trust M) | RoHS2 Compliant | [Infineon RoHS document](https://4donline.ihs.com/images/VipMasterIC/IC/INFN/INFN-E-A0005477335/INFN-E-A0005477335-1.pdf?hkey=6D0214268300F1406B835FE51CB13195) |
| 3 | U2 | 1 | MAX17048G+T10 | Analog Devices (Maxim) | MAX17048G+T10 | ROHS3 Compliant | [Maxim RoHS CoC](https://www.maximintegrated.com/content/dam/files/design/emmi/rohs-certificate-of-compliance.pdf) |
| 4 | U3 | 1 | NCP380HSNAJAAT1G | onsemi | NCP380HSNAJAAT1G | ROHS3 Compliant | [onsemi RoHS statement](https://www.onsemi.com/site/pdf/RoHS-statement.pdf) |
| 5 | U9 | 1 | AP7361C-33E | Diodes Incorporated | AP7361C-33E-13 | ROHS3 Compliant | [Diodes Inc. RoHS3/REACH statement](https://mm.digikey.com/Volume0/opasdata/d220001/medias/docus/6731/Diode%20RoHS3%20REACH%20247%20Jan%202025%20Statement.pdf) |
| 6 | U10 | 1 | MCP73833-FCI/MF | Microchip Technology | MCP73833-FCI/MF | ROHS3 Compliant | [Microchip RoHS CoC](https://ww1.microchip.com/downloads/aemDocuments/documents/corporate-responsibilty/environmental/product-regulatory-information/Semiconductor_EnvCoC.pdf) |
| 7 | U6 | 1 | USBLC6-2SC6 | Goodwork | USBLC6-2SC6 | ROHS3 Compliant | **Missing** – request from Goodwork |
| 8 | U8 | 1 | DW01A | Shenzhen Slkormicro | DW01A | ROHS3 Compliant | **Missing** – request from Slkormicro |
| 9 | U7 | 1 | FS8205A | EVVO | FS8205A | ROHS3 Compliant | **Missing** – request from EVVO |
| 10 | C6,C8,C10,C17,C19,C20,C21,C22,C23,C25,C29,C30 | 12 | 0.1 µF | KEMET (YAGEO) | C0805C104K5RACTU | ROHS3 Compliant | [KEMET part RoHS certificate](https://yageogroup.com/component-documentation/download/rohs/C0805C104K5RACTU.pdf) |
| 11 | C7,C9,C14,C16,C24 | 5 | 10 µF | Samsung Electro-Mechanics | CL21B106KOQNNNE | ROHS3 Compliant | [Samsung MLCC RoHS compliance](https://www.samsungsem.com/files/fileDown.do?key=ed1dfdfa-645e-4252-b707-17f9b1502dd1) |
| 12 | C11 | 1 | 22 µF | Samsung Electro-Mechanics | CL21X226MQQNNNG | ROHS3 Compliant | [Samsung MLCC RoHS compliance](https://www.samsungsem.com/files/fileDown.do?key=ed1dfdfa-645e-4252-b707-17f9b1502dd1) |
| 13 | C18, C26,C27,C28 | 4 | 2.2 µF | Taiyo Yuden | UMK212BB7225KG-T | ROHS3 Compliant | [Taiyo Yuden MLCC RoHS declaration](https://www.yuden.co.jp/en/product/certificate/upload/MLCC-RoHS.pdf) |
| 14 | C15 | 1 | 1 µF | KEMET (YAGEO) | C0805C105K4RACTU | ROHS3 Compliant | [KEMET part RoHS certificate](https://yageogroup.com/component-documentation/download/rohs/C0805C105K4RACTU.pdf) |
| 15 | R1,R11,R14 | 3 | 10 K | Stackpole Electronics | RMCF0805FT10K0 | ROHS3 Compliant | [Stackpole RoHS status list](https://www.seielect.com/catalog/SEI-RoHS_Compliance_Status.pdf) |
| 16 | R8,R9,R15,R16 | 4 | 4.7 K | YAGEO | RC0805FR-074K7L | ROHS3 Compliant | [YAGEO RoHS compliance statement](https://yageogroup.com/content/Resource%20Library/Product%20Environmental%20Compliance/RoHS_Compliance_YAGEO.pdf) |
| 17 | R3,R4 | 2 | 5.1 K | YAGEO | RC0805JR-075K1L | ROHS3 Compliant | [YAGEO RoHS compliance statement](https://yageogroup.com/content/Resource%20Library/Product%20Environmental%20Compliance/RoHS_Compliance_YAGEO.pdf) |
| 18 | R6,R7 | 2 | 1 K | Vishay Dale | CRCW08051K00FKEA | ROHS3 Compliant | [Vishay RoHS statement](https://www.vishay.com/doc?49455) |
| 19 | R5 | 1 | 2 K | Panasonic | ERJ-P06F2001V | ROHS3 Compliant | [Panasonic RoHS certificate](https://industrial.panasonic.com/cdbs/www-cgi/auto-publishing.cgi?type=ROSU&region=WW&layer=4&cd=AOA0002) |
| 20 | R12 | 1 | 4 K | Stackpole Electronics | RNCF0805BTY4K00 | ROHS3 Compliant | [Stackpole RoHS status list](https://www.seielect.com/catalog/SEI-RoHS_Compliance_Status.pdf) |
| 21 | R13 | 1 | 60 K | Vishay Dale | CRCW080559K0FKEA | ROHS3 Compliant | [Vishay RoHS statement](https://www.vishay.com/doc?49455) |
| 22 | R10 | 1 | 390 K | YAGEO | RC0805JR-07390KL | ROHS3 Compliant | [YAGEO RoHS compliance statement](https://yageogroup.com/content/Resource%20Library/Product%20Environmental%20Compliance/RoHS_Compliance_YAGEO.pdf) |
| 23 | FB2 | 1 | Ferrite bead 600 Ω @100 MHz | Murata | BLM21PG600SN1D | ROHS3 Compliant | [Murata EMIFIL RoHS certificate](https://www.murata.com/-/media/webrenewal/products/emc/emifil/certificate/r-eu-rohs-certificate-emi-emifil.ashx?la=en) |
| 24 | D1 | 1 | Red LED | Lite-On | LTST-C190KRKT | ROHS3 Compliant | [Lite-On RoHS compliance](https://optoelectronics.liteon.com/en-global/Page/RoHS) |
| 25 | D1 | 1 | Green LED | Lite-On | LTST-C190GKT | ROHS3 Compliant | [Lite-On RoHS compliance](https://optoelectronics.liteon.com/en-global/Page/RoHS) |
| 26 | SW1-6 | 6 | Tactile switch (XKB TS-1187A) | C&K (Littelfuse) | PTS526 SK15 SMTR2 LFS | ROHS3 Compliant | [Littelfuse RoHS CoC](https://www.littelfuse.com/assetdocs/coc-rohs10-08-18-2022?assetguid=6d7cc3c6-97e3-4940-98fc-322868282297) |
| 27 | SW7 | 1 | Slide switch SPDT | C&K (Littelfuse) | OS102011MS2QN1 | ROHS3 Compliant | [Littelfuse RoHS CoC](https://www.littelfuse.com/assetdocs/coc-rohs10-08-18-2022?assetguid=6d7cc3c6-97e3-4940-98fc-322868282297) |
| 28 | DS1 | 1 | OLED-128O064D 128×64 | DLC Display | DLC0096DNOG-W-5 | RoHS Compliant | **Missing** – request from DLC Display |
| 29 | J2 | 1 | USB-C receptacle (HC-TYPE-C-16P-01A) | Same Sky (CUI Devices) | UJ20-C-H-G-SMT-2-P16-TR | RoHS Compliant | [Same Sky RoHS/REACH declaration](https://www.sameskydevices.com/product/resource/material-compliance/uj20-c-h-g-smt-2-p16-tr) |
| 30 | PCB | 1 | PCB | JLCPCB | Bare PCB | [x] | [x] |

*"Part number (as linked)" is the part that the DigiKey link in the BOM points to. The RoHS evidence applies to that part. If you buy a different part number or brand, its evidence must be replaced.*

## 5. Assessment summary

All 29 BOM lines are listed as RoHS compliant by the distributor or manufacturer. One exception: U5 (Infineon SLS32AIA010) is recorded as **RoHS2** only. A manufacturer RoHS document is linked for 25 of the 29 lines. Four lines have a distributor status only: Goodwork USBLC6-2SC6, Slkormicro DW01A, EVVO FS8205A and the DLC OLED. **Conclusion:** most components are expected to be compliant. The RoHS3 status of U5 and the four undocumented parts must be confirmed before signing.

## 6. Open items before the declaration can be signed

1. **U5 Infineon SLS32AIA010MLUSON10XTMC1:** recorded as "RoHS2 Compliant". Confirm from Infineon's product page or material content data that it also meets (EU) 2015/863 (the four phthalates).
2. **U6 USBLC6-2SC6 (Goodwork), U8 DW01A (Slkormicro), U7 FS8205A (EVVO):** no published RoHS documents. Request signed RoHS declarations or SGS test reports from each supplier, or use sources that publish them.
3. **DS1 OLED (DLC Display):** no published RoHS document. Request one from DLC Display.
4. **C&K switches:** the linked Littelfuse RoHS certificate could not be downloaded for verification. Confirm that it covers the C&K part numbers used, or request a part-specific CoC from Littelfuse (envrequests@littelfuse.com).
5. **PCB, solder and assembly:** the BOM has no PCB line. Add the bare PCB and get the fabricator's and assembler's RoHS declarations (section 8).
6. **Battery:** the charger and protection circuit imply a Li-ion/LiPo cell, but no battery is listed. Add it to the BOM. It is covered by the Battery Regulation, not RoHS.
7. Fill in the company details, hardware revision and signatory in sections 1 and 2.


## 8. Manufacturing and assembly

| # | Item | Details | Supplier / product | RoHS evidence | Status |
|---|---|---|---|---|---|
| 1 | Bare PCB | Part no. / rev: `[ ]` · Surface finish: `[ENIG / lead-free HASL]` | `[PCB fab]` | See BOM table, PCB line | `[Open / Closed]` |
| 2 | Assembly | `[In-house / EMS name]` · Lead-free process only, no leaded solder used | `[ ]` | Assembly statement (this table, signed in section 2) | `[Open / Closed]` |
| 3 | Solder paste (reflow) | Lead-free alloy: `[e.g. SAC305]` | `[Manufacturer, product name]` | `[link / file to RoHS CoC]` | `[Open / Closed]` |
| 4 | Solder wire (hand soldering / rework) | Lead-free alloy: `[e.g. SAC305 / Sn99.3Cu0.7]` | `[Manufacturer, product name]` | `[link / file to RoHS CoC]` | `[Open / Closed]` |
| 5 | Flux | `[No-clean / rosin / water-soluble]` | `[Manufacturer, product name]` | `[link / file to RoHS CoC or SDS]` | `[Open / Closed]` |
| 6 | Cleaning agent | `[IPA / none]` | `[ ]` | `[N/A – evaporates, not in product]` | `[Open / Closed / N/A]` |
| 7 | Casing | 3D-printed, food-grade PETG · Colour: `[ ]` | Elegoo PETG `[product name]` | `[link / file to Elegoo RoHS declaration]` | `[Open / Closed]` |
| 8 | Fasteners | None – device is shipped without screws | – | N/A | N/A |
| 9 | Coating / adhesive / thermal pads | `[None / product name]` | `[ ]` | `[link / file or N/A]` | `[Open / Closed / N/A]` |
| 10 | Labels / marking | `[None / label material, printing method]` | `[ ]` | `[link / file or N/A]` | `[Open / Closed / N/A]` |
| 11 | Battery (if supplied) | `[Chemistry, capacity, part no. / N/A]` | `[ ]` | Battery Regulation (EU) 2023/1542 – outside RoHS scope | `[Open / Closed / N/A]` |
| 12 | Cables / accessories shipped | `[None / USB cable etc.]` | `[ ]` | `[link / file or N/A]` | `[Open / Closed / N/A]` |

**Assembly statement:** all assembly and rework of this product is done with lead-free solder and lead-free process materials only. No leaded solder is used, and no part is substituted without updating the BOM table and this document.

## 9. Related regulatory obligations (outside RoHS)

RoHS is one part of CE marking. The following also apply, or should be checked, for this product:

| Requirement | Applicability |
|---|---|
| EMC Directive 2014/30/EU | Applies. EMC testing is needed (the "EMC Test link" column in the BOM is currently empty). |
| Low Voltage Directive 2014/35/EU | Not applicable if the device is supplied only by USB (5 V) or a battery below 50 V AC / 75 V DC. Safety is covered via the General Product Safety Regulation (EU) 2023/988, or via RED art. 3.1a for radio devices. |
| Battery Regulation (EU) 2023/1542 | Applies if a battery is supplied with the product. Batteries are outside the scope of RoHS, but the regulation restricts Hg, Cd and Pb in batteries and sets labelling, removability and take-back requirements. |
| REACH (EC) 1907/2006, Art. 33 | Confirm that no SVHC above 0.1 % w/w is present in the article. Get the REACH statements from suppliers (several are available alongside the RoHS documents above). |
| WEEE 2012/19/EU | Crossed-out wheelie-bin symbol on the product or packaging, producer registration in each EU country of sale, and take-back information. |

## 10. Technical documentation and record keeping

Detailed per object documentation can be found at LINK

## 11. Revision history

| Rev. | Date | Description | Author |
|---|---|---|---|
| 0.1 | 2026-10-03 | Initial draft generated from the BOM in *Muru Product Compliance.xlsx* | `[name]` |
