#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <cstdint>
#include <cstring>
#include <vector>
using namespace std;

const int pc = 15;
const int sp = 14;
const int status = 0;
const int handler = 1;
const int cause = 2;

uint32_t regs[16]; // Registarsko polje: r0-r15
vector<unsigned char> mem;
uint32_t csr[3];
bool halted = false; // Da li je program završen?

// Učitavanje HEX fajla u memoriju
void loadHexFile(const string &filename)
{
  ifstream inFile(filename);
  string line;
  string address;
  string b;
  unsigned long addr;
  unsigned int byt;

  // Ucitaj sadrzaj fajla u memoriju
  if (inFile.is_open())
  {
    while (getline(inFile, line))
    {
      if (line.size() < 1)
        break;
      address = line.substr(0, 8);
      addr = stoul(address, nullptr, 16);
      int i = 10;
      while (i < line.size())
      {
        b = line.substr(i, 2);
        byt = stoi(b, nullptr, 16);
        mem[addr++] = byt;
        i += 3;
      }
    }
    inFile.close();
  }
  else
  {
    cerr << "Nije moguce ucitati memoriju!\n";
    exit(-1);
  }
}

uint32_t fetch32(uint32_t addr)
{
  if (addr > 0xfffffffcU)
    return -1; // Sprecava citanje sa vrha adresnog prostora
  unsigned char first = mem[addr];
  unsigned char second = mem[addr + 1];
  unsigned char third = mem[addr + 2];
  unsigned char fourth = mem[addr + 3];
  return (uint32_t)(first | (second << 8) | (third << 16) | (fourth << 24));
}

uint32_t fetchInstr(uint32_t addr)
{
  return (uint32_t)((mem[addr] << 24) | (mem[addr + 1] << 16) | (mem[addr + 2] << 8) | (mem[addr + 3]));
}
void dumpRegs()
{
  cout << "Emulated processor executed halt instruction\n";
  cout << "Emulated processor state:\n";
  for (int i = 0; i < 16; i++)
  {
    printf("r%d=0x%08X ", i, regs[i]);
    if ((i + 1) % 4 == 0)
      cout << "\n";
  }
}

void push_to_stack(uint32_t value)
{
  regs[sp] -= 4;
  mem[regs[sp]] = value & 0xFF;
  mem[regs[sp] + 1] = (value >> 8) & 0xFF;
  mem[regs[sp] + 2] = (value >> 16) & 0xFF;
  mem[regs[sp] + 3] = (value >> 24) & 0xFF;
}

void handle_interrupt(uint32_t src)
{
  // 1️⃣ Ispis poruke (možeš koristiti različite poruke po cause-u)

  switch (src)
  {
  case 1:
    cerr << "Greška: Nelegalna instrukcija!" << endl;
    break;
  case 2:
    cerr << "Prekid: Tajmer!" << endl;
    break;
  case 3:
    cerr << "Prekid: Terminal!" << endl;
    break;
  case 4:
  {
    cout << "Softverski prekid (int)!" << endl;
  }
  break;
  default:
    cerr << "Nepoznat prekid!" << endl;
  }

  // 2️⃣ Spremi status i pc na stek (u tom redosledu!)
  push_to_stack(csr[status]); // status
  push_to_stack(regs[pc]);    // povratna adresa

  // 3️⃣ Isključi prekide (maskiranje)
  csr[status] |= 0; // maskirano sve, npr. ako status koristi bitove za enable/disable

  // 4️⃣ Upiši cause
  csr[cause] = src;
  cout << "Razlog ulaska u prekidnu je sa kodom: " << csr[cause] << endl;

  // 5️⃣ Skoči na handler
  regs[pc] = csr[handler];
}

void decode_and_execute(uint32_t instr)
{
  uint8_t opcode = (instr >> 28) & 0x0F; // opcode = gornjih 4 bita prvog bajta
  uint8_t mode = (instr >> 24) & 0x0F;   // mode = donjih 4 bita prvog bajta

  uint8_t byte1 = (instr >> 16) & 0xFF;
  uint8_t byte2 = (instr >> 8) & 0xFF;
  uint8_t byte3 = instr & 0xFF;

  uint8_t regA = (byte1 >> 4) & 0x0F;
  uint8_t regB = byte1 & 0x0F;
  uint8_t regC = (byte2 >> 4) & 0x0F;

  uint16_t D_unsigned = ((uint16_t)byte3 << 4) | (byte2 & 0x0F);
  int16_t D = (D_unsigned & 0x800) ? (int16_t)(D_unsigned | 0xF000) : (int16_t)D_unsigned;
  // Umesto little-endian interpretacije, obrni redosled bajtova za big-endian:
  /*uint8_t byte0 = (instr) & 0xFF; // Najznačajniji bajt
  uint8_t byte1 = (instr >> 8) & 0xFF;
  uint8_t byte2 = (instr >> 16) & 0xFF;
  uint8_t byte3 = (instr >> 24) & 0xFF; // Najmanje značajan bajt

  // byte0: gornjih 4 bita = opcode, donjih 4 bita = mode
  uint8_t opcode = (byte0 >> 4) & 0x0F;
  uint8_t mode = byte0 & 0x0F;

  uint8_t regA = (byte1 >> 4) & 0x0F;
  uint8_t regB = byte1 & 0x0F;
  uint8_t regC = (byte2 >> 4) & 0x0F;

  uint16_t D_unsigned = ((uint16_t)byte3 << 4) | (byte2 & 0x0F);
  int16_t D = (D_unsigned & 0x800) ? (int16_t)(D_unsigned | 0xF000) : (int16_t)D_unsigned;




    opcode = (instr & 0xf0000000U) >> 28; //Operacioni kod instrukcije
    mode = (instr & 0xf000000U) >> 24; //Mod za instrukcije
    A = (instr & 0xf00000U) >> 20;
    B = (instr & 0xf0000U) >> 16;
    C = (instr & 0xf000U) >> 12; */

  // Debug prikaz (po želji)
  // cout << hex << "Instrukcija: OPC=" << (int)opcode << " MODE=" << (int)mode
  //      << " A=" << (int)regA << " B=" << (int)regB << " C=" << (int)regC << " D=" << D << endl;

  switch (opcode)
  {
  case 0x0: // HALT instrukcija
    // provera formata
    if (instr == 0)
      halted = true;
    else
      handle_interrupt(1);
    break;
  case 0x1: // INT instrukcija
    if (mode == 0)
    {
      handle_interrupt(4); // softverski prekid
    }
    else
    {
      handle_interrupt(1); // neispravan modifikator
    }
    break;

  case 0x2: // CALL instrukcija
  {
    uint32_t target = 0;

    switch (mode)
    {
    case 0x0: // target = dir
      target = regs[regA] + regs[regB] + D;
      break;

    case 0x1: // target = ind
      target = regs[regA] + regs[regB] + D;
      if (target > (UINT32_MAX - 3))
      {
        cerr << "Greška: Adresa za fetch32 van granica!" << endl;
        handle_interrupt(1);
        return;
      }
      target = fetch32(target);

      break;

    default:
      handle_interrupt(1); // nepoznat mod za CALL
      return;
    }

    // Čuvamo povratnu adresu (PC) na stek
    push_to_stack(regs[pc]);
    // Skok na target adresu
    regs[pc] = target;
  }
  break;

  case 0x3: // JMP ILI BRANCH INSTR
  {
    uint32_t target = 0;
    switch (mode)
    {
    case 0x0: // jmp dir
      target = regs[regA] + D;
      break;
    case 0x1: // branch eq dir
      if (regs[regB] == regs[regC])
      {
        target = regs[regA] + D;
      }
      else
      {
        target = 0;
      }
      break;
    case 0x2: // branch neq dir
      if (regs[regB] != regs[regC])
      {
        target = regs[regA] + D;
      }
      else
      {
        target = 0;
      }
      break;
    case 0x3: // branch gt dir
      if ((int8_t)regs[regB] > (int8_t)regs[regC])
      {
        target = regs[regA] + D;
      }
      else
      {
        target = 0;
      }
      break;
    case 0x4: // jmp ind
      target = regs[regA] + D;
      if (target > (UINT32_MAX - 3))
      {
        cerr << "Greška: Adresa za fetch32 van granica!" << endl;
        handle_interrupt(1);
        return;
      }
      target = fetch32(regs[regA] + D);

      break;
    case 0x5: // branch eq ind
      if (regs[regB] == regs[regC])
      {
        target = regs[regA] + D;
        if (target > (UINT32_MAX - 3))
        {
          cerr << "Greška: Adresa za fetch32 van granica!" << endl;
          handle_interrupt(1);
          return;
        }
        target = fetch32(target);
      }
      else
      {
        target = 0;
      }

      break;
    case 0x6: // branch neq ind
      if (regs[regB] != regs[regC])
      {
        target = regs[regA] + D;
        if (target > (UINT32_MAX - 3))
        {
          cerr << "Greška: Adresa za fetch32 van granica!" << endl;
          handle_interrupt(1);
          return;
        }
        target = fetch32(target);
      }
      else
      {
        target = 0;
      }

      break;
    case 0x7: // branch gt ind
      if ((int8_t)regs[regB] > (int8_t)regs[regC])
      {
        target = regs[regA] + D;
        if (target > (UINT32_MAX - 3))
        {
          cerr << "Greška: Adresa za fetch32 van granica!" << endl;
          handle_interrupt(1);
          return;
        }
        target = fetch32(target);
      }
      else
      {
        target = 0;
      }

      break;
    default:
      handle_interrupt(1);
      break;
    }
    if (target != 0)
    {
      regs[pc] = target;
    }
  }
  break;

  case 0x4: // XCNHG INSTR
    if (mode != 0 || regA != 0 || D != 0)
      handle_interrupt(1);
    else
    {
      int temp = regs[regB];
      regs[regB] = regs[regC];
      regs[regC] = temp;
    }
    break;

  case 0x5: // ARITM INSTR
  {
    if (D != 0)
    {
      handle_interrupt(1);
    }
    else
    {
      switch (mode)
      {
      case 0x0: // add
        regs[regA] = regs[regB] + regs[regC];
        break;
      case 0x1: // sub
        regs[regA] = regs[regB] - regs[regC];
        break;
      case 0x2: // mul
        regs[regA] = regs[regB] * regs[regC];
        break;
      case 0x3: // div
        if (regs[regC] == 0)
        {
          cerr << "Greška: Deljenje nulom!" << endl;
          handle_interrupt(1); // možeš napraviti novi kod za aritmetičke greške
        }
        else
        {
          regs[regA] = regs[regB] / regs[regC];
        }
        break;

      default:
        handle_interrupt(1); // Nepoznat mod aritmetičke instrukcije
        break;
      }
    }
  }
  break;
  case 0x6: // LOG INSTR
  {
    if (D != 0)
    {
      handle_interrupt(1);
    }
    else
    {
      switch (mode)
      {
      case 0x0: // Not
        regs[regA] = ~regs[regB];
        break;
      case 0x1: // and
        regs[regA] = regs[regB] & regs[regC];
        break;
      case 0x2: // or
        regs[regA] = regs[regB] | regs[regC];
        break;
      case 0x3: // xor
        regs[regA] = regs[regB] ^ regs[regC];
        break;

      default:
        handle_interrupt(1); // Nepoznat mod logicke instrukcije
        break;
      }
    }
  }
  break;

  case 0x7: // SHIFT INSTR
  {
    if (D != 0)
    {
      handle_interrupt(1);
    }
    else
    {
      switch (mode)
      {
      case 0x0: // SHL: reg[A] = reg[B] << reg[C]
        regs[regA] = regs[regB] << regs[regC];
        break;

      case 0x1: // SHR: reg[A] = reg[B] >> reg[C] (logički pomeraj udesno)
        regs[regA] = regs[regB] >> regs[regC];
        break;

      default:
        handle_interrupt(1); // Nepoznat mod shift instrukcije
        break;
      }
    }
  }
  break;

  case 0x8: // ST instrukcija
  {
    uint32_t addr;

    switch (mode)
    {
    case 0x0: // mem32[reg[A] + reg[B] + D] = reg[C]
      addr = regs[regA] + regs[regB] + D;
      break;

    case 0x1: // reg[A] += D; mem32[reg[A]] = reg[C]
      cout << "U registar " << regA << " dodajemo D: " << D << " i na addr: " << addr << "upisujemo u memoriju: " << regs[regC] << endl;
      regs[regA] += D;
      addr = regs[regA];
      break;

    case 0x2: // mem32[mem32[reg[A] + reg[B] + D]] = reg[C]
    {
      uint32_t innerAddr = regs[regA] + regs[regB] + D;
      if (innerAddr > UINT32_MAX - 3)
      {
        cerr << "Greška: Adresa za fetch32 van granica!" << endl;
        handle_interrupt(1);
        return;
      }
      addr = fetch32(innerAddr);
    }
    break;

    default:
      handle_interrupt(1); // Neispravan mod
      return;
    }

    // Provera da li je adresa u granicama memorije
    if (addr > UINT32_MAX - 3)
    {
      cerr << "Greška: Adresa za upis ST instrukcije van opsega memorije!" << endl;
      handle_interrupt(1);
      return;
    }

    // Upis u memoriju (little-endian)
    mem[addr] = regs[regC] & 0xFF;
    mem[addr + 1] = (regs[regC] >> 8) & 0xFF;
    mem[addr + 2] = (regs[regC] >> 16) & 0xFF;
    mem[addr + 3] = (regs[regC] >> 24) & 0xFF;
  }
  break;

  case 0x9: // LD instrukcija
  {
    uint32_t addr;

    switch (mode)
    {
    case 0x0: // gpr[A] = csr[B]
      if (regB > 2)
      {
        handle_interrupt(1);
      }
      else
      {
        if (regA != 0)
        {
          regs[regA] = csr[regB];
        }
      }
      break;

    case 0x1: // gpr[A]<=gpr[B]+D;
      if (regA != 0)
      {
        regs[regA] = regs[regB] + D;
        dumpRegs();
      }
      break;

    case 0x2: // gpr[A]<=mem32[gpr[B]+gpr[C]+D];
    {
      addr = regs[regB] + regs[regC] + D;
      cout << "Adresa na kojoj se nalazi podatak: " << addr << endl;

      // Provera da li adresa prelazi opseg memorije
      if (addr > UINT32_MAX - 3)
      {
        cerr << "Greška: LD adresa van opsega!" << endl;
        handle_interrupt(1);
        return;
      }

      regs[regA] = fetch32(addr);
      cout << "Procitani podatak: " << regs[regA] << endl;
    }
    break;

    case 0x3: // gpr[A]<=mem32[gpr[B]]; gpr[B]<=gpr[B]+D;
    {
      addr = regs[regB];

      if (addr > UINT32_MAX - 3)
      {
        cerr << "Greška: LD adresa van opsega!" << endl;
        handle_interrupt(1);
        return;
      }

      regs[regA] = fetch32(addr);
      regs[regB] += D;
    }
    break;

    case 0x4: // csr[A]<=gpr[B];
    {
      if (regA > 2)
      {
        handle_interrupt(1);
      }
      csr[regA] = regs[regB];
    }
    break;

    case 0x5: // csr[A]<=csr[B]|D;
    {
      if (regA > 2 || regB > 2)
      {
        handle_interrupt(1);
      }
      csr[regA] = csr[regB] | D;
    }
    break;

    case 0x6: // csr[A]<=mem32[gpr[B]+gpr[C]+D];
    {
      addr = regs[regB] + regs[regC] + D;

      if (addr > UINT32_MAX - 3)
      {
        cerr << "Greška: LD adresa van opsega!" << endl;
        handle_interrupt(1);
        return;
      }

      csr[regA] = fetch32(addr);
    }
    break;

    case 0x7: // csr[A]<=mem32[gpr[B]]; gpr[B]<=gpr[B]+D;
    {
      addr = regs[regB];

      if (addr > UINT32_MAX - 3)
      {
        cerr << "Greška: LD adresa van opsega!" << endl;
        handle_interrupt(1);
        return;
      }

      csr[regA] = fetch32(addr);
      regs[regB] += D;
    }
    break;

    default:
      handle_interrupt(1); // Nepoznat mod
      return;
    }

    /*// Provera da li je adresa validna za čitanje
    if (addr > UINT32_MAX - 3)
    {
      cerr << "Greška: Adresa za čitanje LD instrukcije van opsega memorije!" << endl;
      handle_interrupt(1);
      return;
    }

    // Čitanje vrednosti iz memorije u registar
    regs[regA] = fetch32(addr);*/
  }
  break;
//break;
  default:
    cout << "Prekid zbog instrukcije: " << instr << endl;
    handle_interrupt(5);
    break;
  }
}

void freeMem()
{
}

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    cerr << "Nije zadat hex fajl kao argument" << endl;
    return 1;
  }

  // inicijalizacija
  memset(csr, 0, sizeof(csr));
  memset(regs, 0, sizeof(regs));
  // memset(mem, 0, sizeof(mem));
  mem.reserve(4294967296);
  regs[pc] = 0x40000000;
  // csr[status] = 3;

  loadHexFile(argv[1]);

  while (!halted)
  {
    uint32_t instr = fetchInstr(regs[pc]);
    printf("PC=0x%08X | Instrukcija=0x%08X\n", regs[pc], instr);
    regs[pc] += 4;
    decode_and_execute(instr);
  }

  uint32_t cnt = 0;

  /*for (cnt; cnt < 100; cnt += 4)
  {
    printf("PC=0x%08X | Instrukcija=0x%08X\n", cnt, fetch32(cnt));
  }*/

  // printf("PC=0x%08X | Instrukcija=0x%08X\n", regs[pc], mem[cnt]);

  dumpRegs();

  // freeMem();

  return 0;
}
