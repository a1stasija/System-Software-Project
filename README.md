# System Software Project

Implementacija toolchain-a (asembler, linker i emulator) za **32-bitnu apstraktnu mašinu** sa von Neumann arhitekturom. Projekat je rađen u sklopu predmeta *Sistemski softver* i pokriva ceo put od asemblerskog koda (`.s`) do izvršavanja programa u emulatoru.

## Sadržaj

- [Pregled](#pregled)
- [Arhitektura sistema](#arhitektura-sistema)
- [Struktura projekta](#struktura-projekta)
- [Zahtevi](#zahtevi)
- [Build](#build)
- [Upotreba](#upotreba)
- [Asembler](#asembler)
- [Linker](#linker)
- [Emulator](#emulator)
- [Skup instrukcija](#skup-instrukcija)
- [Načini adresiranja](#načini-adresiranja)
- [Primer](#primer)

## Pregled

Toolchain se sastoji od tri međusobno povezana alata:

1. **Asembler** — transformiše asemblerski izvorni kod (`.s`) u objektne fajlove (`.o`). Implementiran je u dva prolaza i koristi *flex* za leksičku analizu i *bison* za sintaksnu analizu.
2. **Linker** — povezuje više objektnih fajlova u jedan izvršni fajl (`.hex`), razrešava simbole i obavlja relokacije. Podržava ručno mapiranje sekcija na konkretne memorijske adrese (`-place=section@addr`).
3. **Emulator** — učitava `.hex` fajl, simulira rad procesora i izvršava program sa podrškom za prekide i memorijski mapiran ulaz/izlaz (terminal, timer).

## Arhitektura sistema

- **Veličina reči:** 32 bita
- **Registri opšte namene:** `r0`–`r15` (gde su `r14 = sp`, `r15 = pc`)
- **Kontrolni i statusni registri (CSR):** `status`, `handler`, `cause`
- **Adresni prostor:** 32-bitni
- **Memorijski mapiran I/O:** terminal (`term_in`, `term_out`) i timer (`tim_cfg`)
- **Prekidi:** softverski (`int`), hardverski (timer, terminal), kao i izuzeci

## Struktura projekta

```
.
├── inc/                    # Header fajlovi
│   ├── assembler.hpp       # Strukture: SymbolTable, Section, RelocationTable, Arguments…
│   └── linker.hpp          # Strukture za linkovanje i razrešavanje simbola
├── src/                    # C++ izvorni fajlovi
│   ├── main.cpp            # Ulazna tačka asemblera (parsiranje argumenata)
│   ├── assembler.cpp       # Implementacija asemblera u dva prolaza
│   ├── linker.cpp          # Implementacija linkera
│   └── emulator.cpp        # Implementacija emulatora
├── misc/                   # Generatori parsera
│   ├── lexer.l             # Flex specifikacija leksera
│   └── parser.y            # Bison gramatika
├── tests/                  # Testovi sa .s fajlovima
├── makefile                # Build sistem
├── start.sh                # Skripta za end-to-end pokretanje testa
└── README.md
```

## Zahtevi

- `g++` (sa podrškom za C++11 ili noviji)
- `flex`
- `bison`
- GNU Make
- Linux okruženje (testirano)

Instalacija na Ubuntu/Debian:

```bash
sudo apt update
sudo apt install build-essential flex bison
```

## Build

Iz korenskog direktorijuma projekta:

```bash
make
```

Komanda gradi sva tri izvršna fajla (`assembler`, `linker`, `emulator`). Pojedinačni build je takođe moguć:

```bash
make build-assembler
make build-linker
make build-emulator
```

Čišćenje generisanih fajlova:

```bash
make clean
```

## Upotreba

### Asembler

```bash
./assembler -o izlaz.o ulaz.s
```

Ako se `-o` opcija ne navede, izlazni fajl se izvodi iz imena ulaznog (`ulaz.s → ulaz.o`).

### Linker

```bash
./linker -hex \
    -place=text@0x40000000 \
    -place=data@0xF0000000 \
    -o program.hex \
    file1.o file2.o file3.o
```

Opcije:

- `-hex` — generiše `.hex` izlaz spreman za emulator
- `-place=<sekcija>@<adresa>` — postavlja sekciju na konkretnu adresu u memoriji
- `-o <fajl>` — ime izlaznog fajla

### Emulator

```bash
./emulator program.hex
```

Emulator počinje izvršavanje od adrese `0x40000000` i radi dok ne naiđe na instrukciju `halt`.

### Pokretanje kompletnog test scenarija

```bash
chmod +x start.sh
./start.sh
```

## Skup instrukcija

| Kategorija | Instrukcije |
|------------|-------------|
| Kontrola toka | `halt`, `int`, `iret`, `call`, `ret`, `jmp` |
| Uslovni skokovi | `beq`, `bne`, `bgt` |
| Stek | `push`, `pop` |
| Podaci | `ld`, `st`, `xchg` |
| Aritmetičke | `add`, `sub`, `mul`, `div` |
| Logičke | `not`, `and`, `or`, `xor` |
| Pomeranje | `shl`, `shr` |
| CSR pristup | `csrrd`, `csrwr` |

### Direktive asemblera

- `.global <simbol>` — označava simbol kao globalan
- `.extern <simbol>` — deklariše eksterni simbol
- `.section <ime>` — definiše sekciju
- `.word <vrednost>` — alocira 4 bajta sa datom vrednošću
- `.skip <broj>` — preskače dati broj bajtova (zero-fill)
- `.end` — kraj asemblerskog fajla

## Načini adresiranja

| Mod | Sintaksa | Opis |
|-----|----------|------|
| Neposredni (literal) | `$<literal>` | Konstantna vrednost |
| Neposredni (simbol) | `$<simbol>` | Adresa simbola kao vrednost |
| Memorijski direktni (literal) | `<literal>` | Sadržaj memorije na adresi |
| Memorijski direktni (simbol) | `<simbol>` | Sadržaj memorije na adresi simbola |
| Registarski direktni | `%<reg>` | Vrednost iz registra |
| Registarski indirektni | `[%<reg>]` | Sadržaj memorije sa adrese u registru |
| Reg. indirektni sa pomerajem (literal) | `[%<reg> + <literal>]` | Pomerena adresa |
| Reg. indirektni sa pomerajem (simbol) | `[%<reg> + <simbol>]` | Pomerena adresa preko simbola |

## Primer

`hello.s`:

```asm
.global main

.section my_code
main:
    ld $0x1, %r1
    ld $0x2, %r2
    add %r1, %r2
    halt
.end
```

Pokretanje:

```bash
./assembler -o hello.o hello.s
./linker -hex -place=my_code@0x40000000 -o hello.hex hello.o
./emulator hello.hex
```

## Implementacioni detalji

- **Asembler** koristi *literal pool* po sekcijama za smeštanje literala koji ne mogu da stanu direktno u instrukciju (12-bitno polje pomeraja). Reference koje se ne mogu razrešiti u prvom prolazu se beleže u *forward reference* tabelu i razrešavaju u drugom prolazu.
- **Linker** radi razrešavanje simbola između modula, spaja istoimene sekcije i primenjuje relokacije (apsolutne i PC-relativne) na osnovu konačnih adresa sekcija.
- **Emulator** implementira fetch–decode–execute ciklus, čuva CPU stanje (16 GPR-ova + CSR-ovi), simulira memoriju kao mapu i obrađuje prekide na osnovu `cause` i `handler` registara.

## Autor

Projekat: *a1stasija* — vežba iz predmeta Sistemski softver.
