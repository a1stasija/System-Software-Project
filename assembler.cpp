#include "../inc/assembler.hpp"
#include <fstream>

using namespace std;

int SymbolTableEntry::cnt = 0;
map<int, Section *> *allSections = new map<int, Section *>();
map<string, SymbolTableEntry *> *symbolTable = new map<string, SymbolTableEntry *>();
map<string, LiteralPoolEntry *> *literalPool = new map<string, LiteralPoolEntry *>();
vector<string> literalInsertionOrder;
extern string outputName;

Section *currSection = nullptr;
int locationCounter = 0;

/*--- POMOCNE FUNKCIJE ---*/

void write_binary_output(const string& filename) {
  ofstream out(filename, ios::binary);
  if (!out) {
    cerr << "Greska: Ne mogu da otvorim binarni fajl za pisanje!" << endl;
    return;
  }

  // 1. Sekcije
  size_t numSections = allSections->size();
  out.write((char*)&numSections, sizeof(size_t));

  for (map<int, Section*>::iterator it = allSections->begin(); it != allSections->end(); ++it) {
    Section* sec = it->second;
    size_t nameLen = sec->name.size();
    out.write((char*)&nameLen, sizeof(size_t));
    out.write(sec->name.c_str(), nameLen);

    out.write((char*)&sec->idSymbolTable, sizeof(unsigned int));
    unsigned int codeSize = sec->code->size();
    out.write((char*)&codeSize, sizeof(unsigned int));
    out.write((char*)sec->code->data(), codeSize);
  }

  // 2. Tabela simbola
  size_t numSymbols = symbolTable->size();
  out.write((char*)&numSymbols, sizeof(size_t));

  for (map<string, SymbolTableEntry*>::iterator it = symbolTable->begin(); it != symbolTable->end(); ++it) {
    const string& name = it->first;
    SymbolTableEntry* sym = it->second;

    size_t nameLen = name.size();
    out.write((char*)&nameLen, sizeof(size_t));
    out.write(name.c_str(), nameLen);

    out.write((char*)&sym->id, sizeof(unsigned int));
    out.write((char*)&sym->ndx, sizeof(unsigned int));
    out.write((char*)&sym->value, sizeof(unsigned int));
    unsigned int bind = (unsigned int)sym->bind;
    out.write((char*)&bind, sizeof(unsigned int)); // LCL=0, GLBL=1, EXTRN=2
  }

  // 3. Tabele relokacija
  size_t numRelocTables = allSections->size();
  out.write((char*)&numRelocTables, sizeof(size_t));

  for (map<int, Section*>::iterator it = allSections->begin(); it != allSections->end(); ++it) {
    Section* sec = it->second;
    vector<RelocationTableEntry*>& relocs = *sec->relocationTableForSection;
    size_t numRelocs = relocs.size();
    out.write((char*)&numRelocs, sizeof(size_t));

    for (size_t i = 0; i < numRelocs; ++i) {
      RelocationTableEntry* rel = relocs[i];

      // Nadji ime simbola po ID-ju
      string symName = "";
      for (map<string, SymbolTableEntry*>::iterator symIt = symbolTable->begin(); symIt != symbolTable->end(); ++symIt) {
        if (symIt->second->id == rel->idSymbol) {
          symName = symIt->first;
          break;
        }
      }

      size_t symNameLen = symName.size();
      out.write((char*)&symNameLen, sizeof(size_t));
      out.write(symName.c_str(), symNameLen);

      out.write((char*)&rel->idSymbol, sizeof(unsigned int));
      out.write((char*)&rel->section, sizeof(unsigned int));
      out.write((char*)&rel->offset, sizeof(unsigned int));
      out.write((char*)&rel->addend, sizeof(unsigned int));
      unsigned int relType = (uint32_t)rel->type;
      out.write((char*)&relType, sizeof(unsigned int));
    }
  }

  out.close();
  //cout << "Binarni izlaz upisan u fajl '" << filename << "'" << endl;
}



void printSymbolTable()
{
  cout << "===== Symbol Table =====\n";
  if (symbolTable->empty())
  {
    cout << "(Tabela simbola je prazna)\n";
    return;
  }

  for (const auto &entry : *symbolTable)
  {
    entry.second->printSymbol();
  }
  cout << "========================\n";
}

void printFlink(const vector<ForwardReferenceTableEntry *> *flink)
{
  if (!flink || flink->empty())
  {
    cout << "(Nema forward referenci)\n";
    return;
  }

  cout << "===== Forward References =====\n";
  for (const auto &ref : *flink)
  {
    cout << "-> Section: " << ref->section
         << ", Offset: " << ref->offset
         << ", Type: " << (ref->type == 0 ? "PC Rel" : "Absolute")
         << "\n";
  }
  cout << "==============================\n";
}

void printLiteralPool()
{
  cout << "--- Literal Pool ---" << endl;
  for (auto &entry : *allSections)
  {
    Section *section = entry.second;

    if (section->literalPoolBase == -1 || section->literalPoolSize == 0)
      continue;

    cout << "Sekcija: " << section->name
         << " (ID: " << section->idSymbolTable << ") -> "
         << "literalPoolBase: " << section->literalPoolBase
         << ", literalPoolSize: " << section->literalPoolSize << endl;

    int base = section->literalPoolBase;
    int size = section->literalPoolSize;

    if (section->code->size() < static_cast<size_t>(base + size))
    {
      cerr << "Greska: Literal pool izlazi izvan opsega sekcije '" << section->name << "'!" << endl;
      continue;
    }

    for (int i = base; i < base + size; i += 4)
    {
      printf("  0x%04X: ", i);
      for (int j = 0; j < 4 && i + j < base + size; ++j)
      {
        printf("%02X ", static_cast<unsigned char>((*section->code)[i + j]));
      }
      cout << endl;
    }
  }
  cout << "---------------------" << endl;
}

void printRelocationTable()
{
  cout << "===== Relocation Table =====\n";

  bool isEmpty = true;
  for (const auto &secEntry : *allSections)
  {
    Section *section = secEntry.second;
    if (!section->relocationTableForSection->empty())
    {
      isEmpty = false;
      cout << "Relocations for Section: " << section->name << "\n";
      for (const auto &entry : *(section->relocationTableForSection))
      {
        entry->print();
      }
      cout << "--------------------------\n";
    }
  }

  if (isEmpty)
  {
    cout << "(Relokaciona tabela je prazna)\n";
  }

  cout << "============================\n";
}

void printAllSections()
{
  cout << "===== All Sections =====\n";
  if (allSections->empty())
  {
    cout << "(Nema definisanih sekcija)\n";
    return;
  }

  for (const auto &entry : *allSections)
  {
    Section *section = entry.second;
    section->print();
    cout << "--------------------------\n";
  }
  cout << "=========================\n";
}

void write_to_memory_data(int sectionId, int offset, int value)
{
  // Pronalazimo sekciju sa datim ID-jem
  Section *section = nullptr;
  for (auto &entry : *allSections)
  {
    if (entry.second->idSymbolTable == sectionId)
    {
      section = entry.second;
      break;
    }
  }

  // Ako sekcija nije pronadjena, ispisujemo gresku
  if (!section)
  {
    cerr << "Greska: Sekcija sa ID " << sectionId << " ne postoji!" << endl;
    return;
  }

  if (!section->code)
  {
    cerr << "Greska: Sekcija '" << section->name << "' nema alociranu memoriju!" << endl;
    return;
  }

  if (offset + 3 >= section->code->size())
  {
    section->code->resize(offset + 4, 0);
  }

  // Upis vrednosti u memoriju (pretpostavljamo Little Endian format)
  (*section->code)[offset] = (value & 0xFF);
  (*section->code)[offset + 1] = ((value >> 8) & 0xFF);
  (*section->code)[offset + 2] = ((value >> 16) & 0xFF);
  (*section->code)[offset + 3] = ((value >> 24) & 0xFF);
}

void write_to_memory_data_long(int sectionId, int offset, unsigned long value)
{
  // Pronalazimo sekciju sa datim ID-jem
  uint32_t truncated = static_cast<uint32_t>(value);

  Section *section = nullptr;
  for (auto &entry : *allSections)
  {
    if (entry.second->idSymbolTable == sectionId)
    {
      section = entry.second;
      break;
    }
  }

  // Ako sekcija nije pronadjena, ispisujemo gresku
  if (!section)
  {
    cerr << "Greska: Sekcija sa ID " << sectionId << " ne postoji!" << endl;
    return;
  }

  if (!section->code)
  {
    cerr << "Greska: Sekcija '" << section->name << "' nema alociranu memoriju!" << endl;
    return;
  }

  if (offset + 3 >= section->code->size())
  {
    section->code->resize(offset + 4, 0);
  }

  // Upis vrednosti u memoriju (pretpostavljamo Little Endian format)
  (*section->code)[offset] = static_cast<unsigned char>(truncated & 0xFF);
  (*section->code)[offset + 1] = static_cast<unsigned char>((truncated >> 8) & 0xFF);
  (*section->code)[offset + 2] = static_cast<unsigned char>((truncated >> 16) & 0xFF);
  (*section->code)[offset + 3] = static_cast<unsigned char>((truncated >> 24) & 0xFF);
}

int calculate_addend(int relocationType, SymbolTableEntry *symbol)
{
  int addend = 0; // Pocetni addend

  // Ako je simbol lokalan, dodaj njegov offset (value iz tabele simbola)
  if (symbol->bind == 1)
  { // 1 = lokalni simbol
    addend += symbol->value;
  }

  // Ako je relokacija PC relativna (type == 0), dodaj -4
  if (relocationType == 0)
  {
    addend -= 4;
  }

  return addend;
}

int calculate_reloc_idSymbol(SymbolTableEntry *symbol)
{
  if (symbol->bind == 1)
  { // Lokalan simbol -> koristimo sekciju
    return symbol->ndx;
  }
  else if (symbol->bind == 0 || symbol->bind == 2)
  { // Globalan ili extern
    return symbol->id;
  }
  else
  {
    cerr << "Greska: Nepoznata vrednost bind za simbol '" << symbol->name << "'" << endl;
    return -1;
  }
}

void printArguments(Arguments *args)
{
  if (!args)
  {
    cout << "(nema argumenata)" << endl;
    return;
  }
  cout << "Adresa mode: " << args->addrMode << " | Argumenti: ";
  for (size_t i = 0; i < args->argName->size(); i++)
  {
    cout << *(args->argName->at(i)) << " (" << args->argType->at(i) << ") ";
  }
  cout << endl;
}
/* ===================================================================================================== */

/* SIMBOLI */
void process_label(string *label)
{
  if (!label)
    return;

  // da li je vec deklarisana

  string symbol = *label;

  auto it = symbolTable->find(symbol);
  if (it != symbolTable->end())
  { // vec ga ima u tabeli simbola

    if (it->second->bind == 2)
    { //vise nije implicitno extern nego je local
      it->second->bind = 1;
    }
    if (it->second->value != -1)
    {
      cerr << "Greska: Simbol '" << symbol << "' ne moze biti dva puta definisan!" << endl;
      exit(-1);
    }
    // dopuni ulaz u tabeli simbola
    it->second->value = locationCounter;
    it->second->type = 1;
    it->second->ndx = currSection->idSymbolTable;

    // BACKPATCHING se radi u .end
  }
  else
  { // nema ga jos u tabeli simbola sto znaci da nije do sada deklarisan ili koriscen
    symbolTable->insert({symbol, new SymbolTableEntry(symbol, 1, 1, currSection->idSymbolTable, locationCounter, true)});
  }
}

/* DIREKTIVE */
void process_END_DIR()
{
  //AKO POSTOJI TRENUTNA SEKCIJA -> UGRADJUJEMO BAZEN LITERALA
  if (currSection)
  {
    currSection->literalPoolBase = locationCounter;
    currSection->literalPoolSize = literalPool->size() * 4;

    if (!literalPool->empty())
    {
      for (const string &key : literalInsertionOrder)
      {
        auto it = literalPool->find(key);
        if (it == literalPool->end())
          continue;

        LiteralPoolEntry *literalEntry = it->second;

        // PATCHUJ FRT ZA TAJ LITERAL/SIMBOL
        for (ForwardReferenceTableEntry *ref : *(literalEntry->flink))
        {
          Section *targetSection = currSection;

          if (!targetSection)
          {
            cerr << "Greska: Sekcija sa ID " << ref->section << " ne postoji!" << endl;
            continue;
          }

          int relocValue = locationCounter - (ref->offset - 2) - 4;

          if (ref->offset + 1 >= targetSection->code->size())
          {
            cerr << "Greska: Offset " << ref->offset << " je van opsega memorije u sekciji '" << targetSection->name << "'!" << endl;
            continue;
          }

          char relocHigh;
          char relocLow;
          if (literalEntry->isSymbol)
          {
            auto sym = symbolTable->find(key);
            if (sym == symbolTable->end())
            {
              cerr << "Greska .END1: Simbol '" << key << "' nije pronadjen u tabeli simbola!" << endl;
              continue;
            }

            if (sym->second->value != -1 && sym->second->ndx == currSection->idSymbolTable)
            {
              // pregazi opcode tako da bude pc rel
              relocValue = sym->second->value - (ref->offset - 2) - 4; // PROVERI FORMULU
              // cout << "VREDNOST SIMBOLA: " << sym->second->value << " , VREDNOST ref->offset: " << ref->offset << endl;
              (*targetSection->code)[ref->offset - 2] = ref->patchCode1;
              //(*targetSection->code)[ref->offset - 1] = ref->patchCode2;
            }
          }
          relocHigh = (relocValue >> 4) & 0xFF;
          relocLow = relocValue & 0x0F;
          (*targetSection->code)[ref->offset] = ((*targetSection->code)[ref->offset] | relocLow); // 0x0X
          (*targetSection->code)[ref->offset + 1] = relocHigh;                                    // 0xXX
        }

        // UPIS VREDNOSTI U MEMORIJU
        if (!literalEntry->isSymbol)
        {
          unsigned long value = stoul(key, nullptr, 0);
          write_to_memory_data_long(currSection->idSymbolTable, locationCounter, value);
          // cout << "Upisujemo vrednost literala u bazen: " << value << "za key: " << key << endl;
        }
        else
        {
          auto symIt = symbolTable->find(key);
          if (symIt == symbolTable->end())
          {
            cerr << "Greska: Simbol '" << key << "' nije pronadjen u tabeli simbola!" << endl;
            write_to_memory_data(currSection->idSymbolTable, locationCounter, 0);
            locationCounter += 4;
            continue;
          }

          SymbolTableEntry *symbol = symIt->second;

          if (symbol->value != -1 && symbol->ndx == currSection->idSymbolTable)
          {
            currSection->size -= 4;
            currSection->literalPoolSize -= 4;
            // cout << "Ne pisemo vrednost simbola u bazen jer je patched up: " << key << endl;
            continue;
          }
          else
          {
            // Globalan ili simbol iz druge sekcije -> relokacija
            // cout << "Ostavljamo mesta za vrednost simbola koji ce linker prepraviti u bazenu za key: " << key << endl;
            write_to_memory_data(currSection->idSymbolTable, locationCounter, 0);

            if (symbol->value != -1)
            {
              RelocationTableEntry *relocEntry = new RelocationTableEntry(
                  locationCounter,
                  1, // apsolutna
                  calculate_addend(1, symbol),
                  currSection->idSymbolTable,
                  calculate_reloc_idSymbol(symbol));

              currSection->relocationTableForSection->push_back(relocEntry);
            }
            else
            {
              // Dodaj u symbol->flink
              symbol->flink->push_back(new ForwardReferenceTableEntry(currSection->idSymbolTable, locationCounter, 1));
            }
          }
        }

        locationCounter += 4;
      }
    }

    currSection->size = locationCounter;
    literalPool->clear();
    literalInsertionOrder.clear();
  }

  //  BACKPATCHING ZA OSTALE FORWARD REFERENCE (mora da ostane zbog .word direktive)
  for (auto &entry : *symbolTable)
  {
    SymbolTableEntry *symbol = entry.second;

    for (auto it = symbol->flink->begin(); it != symbol->flink->end();)
    {
      ForwardReferenceTableEntry *forwardRef = *it;

      if (symbol->value != -1 && symbol->ndx == forwardRef->section && forwardRef->type == 0 && symbol->bind == 1)
      {
        // cout << "Upis u memoriju (local symbol PC-relative)" << endl;

        int val = symbol->value - forwardRef->offset - 4;
        write_to_memory_data(forwardRef->section, forwardRef->offset, val);
      }
      else
      {
        // cout << " Kreiraj relokaciju" << endl;

        if (allSections->find(forwardRef->section) != allSections->end())
        {
          Section *sec = (*allSections)[forwardRef->section];

          //cout << "Napravljena relokacija za simbol " << symbol->name << " sa ID: "<< symbol->id<<endl;
          //cout << "relocID: "<<calculate_reloc_idSymbol(symbol)<<endl;

          RelocationTableEntry *relocEntry = new RelocationTableEntry(
              forwardRef->offset,
              forwardRef->type,
              calculate_addend(forwardRef->type, symbol),
              forwardRef->section,
              calculate_reloc_idSymbol(symbol));

            

          sec->relocationTableForSection->push_back(relocEntry);
        }
        else
        {
          cerr << "Greska: Sekcija " << forwardRef->section << " nije pronadjena!" << endl;
        }
      }

      it = symbol->flink->erase(it);
    }
    if(symbol->bind == 0 && symbol->value == -1){
      cerr << "Greska: Globalni simbol " << symbol->name << " nije definisan u fajlu!" << endl;
      exit(-1);
    }
  }

  write_binary_output(outputName);

  // ISPIS ZA TESTIRANJE
  //printSymbolTable();
  //printAllSections();
  //printRelocationTable();
  //printLiteralPool();
  exit(0);
}

void process_GLOBAL_DIR(Arguments *args)
{
  if (!args || !args->argName || !symbolTable)
    return;

  for (const auto &symbolPtr : *args->argName)
  {
    string symbol = *symbolPtr;

    auto it = symbolTable->find(symbol);

    if (it != symbolTable->end())
    {
      // Simbol vec postoji u tabeli -> postavi ga kao GLOBALAN
      it->second->bind = 0; // 0 = GLOBAL
      // cout << "Simbol '" << symbol << "' postavljen kao globalan." << endl;
    }
    else
    {
      // Simbol ne postoji -> dodaj ga kao nedefinisan, ali globalan
      symbolTable->insert({symbol, new SymbolTableEntry(symbol, 0, 1, 0, -1, false)});
      // cout << "Simbol '" << symbol << "' dodat u tabelu kao globalan i nedefinisan." << endl;
    }
  }
}

void process_EXTERN_DIR(Arguments *args)
{
  if (!args || !args->argName || !symbolTable)
    return; // Provera da li tabela postoji

  for (const auto &symbolPtr : *args->argName)
  {
    string symbol = *symbolPtr;

    auto it = symbolTable->find(symbol);

    if (it != symbolTable->end())
    {
      // Simbol postoji, proveravamo da li je vec definisan ili je vec deklarisan kao ne-extern
      if (it->second->ndx != 0 || it->second->bind != 2)
      {
        cerr << "Greska: Simbol '" << symbol << "' ne moze biti EXTERN jer je vec definisan!" << endl;
        continue;
      }
      
    }
    else
    {
      // Simbol ne postoji, koristimo `insert()` da ga dodamo u tabelu
      auto result = symbolTable->insert({symbol, new SymbolTableEntry(symbol, 2, 1, 0, -1, false)});

      if (result.second)
      {
        // cout << "Simbol '" << symbol << "' dodat kao EXTERN u tabelu simbola." << endl;
      }
      else
      {
        cerr << "Greska pri dodavanju simbola '" << symbol << "' u tabelu!" << endl;
      }
    }
  }
}

void process_WORD_DIR(Arguments *args)
{
  if (!args || !args->argName || !args->argType)
  {
    cerr << "Greska: Nevalidni argumenti za .word direktivu!" << endl;
    return;
  }

  if (!currSection || !currSection->code)
  {
    cerr << "Greska: .word direktiva mora biti unutar sekcije!" << endl;
    return;
  }

  for (size_t i = 0; i < args->argName->size(); i++)
  {
    string arg = *(*args->argName)[i];
    int type = (*args->argType)[i];

    if (type == 0)
    { // Literalna vrednost
      unsigned long literalValue;
      try
      {
        literalValue = stoul(arg, nullptr, 0);
      }
      catch (...)
      {
        cerr << "Greska: Nevalidan literal '" << arg << "' u .word direktivi!" << endl;
        continue;
      }

      // Upisujemo literal u memoriju
      write_to_memory_data(currSection->idSymbolTable, locationCounter, literalValue);
    }
    else if (type == 1)
    { // Simbol, U NEKIM SLUC PRAVIS RELOKACIJU U NEKIM PRAVIS FRT
      auto it = symbolTable->find(arg);

      if (it != symbolTable->end() && it->second->value != -1)
      {

        RelocationTableEntry *relocEntry = new RelocationTableEntry(
            locationCounter,
            1,  // Apsolutna relokacija
            -1, // Addend = -1 jer se naknadno izracunava
            currSection->idSymbolTable,
            -1);

        relocEntry->addend = calculate_addend(1, it->second);
        relocEntry->idSymbol = calculate_reloc_idSymbol(it->second);
        currSection->relocationTableForSection->push_back(relocEntry);

        write_to_memory_data(currSection->idSymbolTable, locationCounter, 0);
      }
      else
      {
        // Simbol nije poznat -> Dodajemo ga u tabelu simbola ako vec ne postoji
        if (symbolTable->find(arg) == symbolTable->end())
        {
          symbolTable->insert({arg, new SymbolTableEntry(arg, 2, 1, 0, -1, false)});
        }

        // Dodajemo u tabelu ForwardReferenceTable sa type = 1
        symbolTable->at(arg)->flink->push_back(new ForwardReferenceTableEntry(currSection->idSymbolTable, locationCounter, 1));
        // printFlink(symbolTable->at(arg)->flink);
        write_to_memory_data(currSection->idSymbolTable, locationCounter, 0);
      }
    }
    else if (type == 2)
    { // Registar (nije validan u .word)
      cerr << "Greska: Registar '" << arg << "' nije dozvoljen u .word direktivi!" << endl;
      continue;
    }

    // Pomeramo `locationCounter` za 4 bajta
    locationCounter += 4;
  }
}

void process_SECTION_DIR(string *name)
{
  if (!name || name->empty())
  {
    cerr << "Greska: Sekcija mora imati validno ime!" << endl;
    return;
  }

  // Ako postoji tekuca sekcija, dodajemo njen bazen literala i azuriramo velicinu
  if (currSection)
  {
    currSection->literalPoolBase = locationCounter;
    currSection->literalPoolSize = literalPool->size() * 4;

    if (!literalPool->empty())
    {
      for (const string &key : literalInsertionOrder)
      {
        auto it = literalPool->find(key);
        if (it == literalPool->end())
          continue;

        LiteralPoolEntry *literalEntry = it->second;

        // PATCHUJ FRT ZA TAJ LITERAL/SIMBOL
        for (ForwardReferenceTableEntry *ref : *(literalEntry->flink))
        {
          Section *targetSection = currSection;

          if (!targetSection)
          {
            cerr << "Greska: Sekcija sa ID " << ref->section << " ne postoji!" << endl;
            continue;
          }

          int relocValue = locationCounter - (ref->offset - 2) - 4;

          if (ref->offset + 1 >= targetSection->code->size())
          {
            cerr << "Greska: Offset " << ref->offset << " je van opsega memorije u sekciji '" << targetSection->name << "'!" << endl;
            continue;
          }
          if (literalEntry->isSymbol)
          {
            auto sym = symbolTable->find(key);
            if (sym == symbolTable->end())
            {
              cerr << "Greska: Simbol '" << key << "' nije pronadjen u tabeli simbola!" << endl;
              continue;
            }

            if (sym->second->value != -1 && sym->second->ndx == currSection->idSymbolTable)
            {
              // pregazi opcode tako da bude pc rel jer se u medjuvremenu saznalo da su u istoj sekciji
              relocValue = sym->second->value - (ref->offset - 2) - 4;
              (*targetSection->code)[ref->offset - 2] = ref->patchCode1;
              //(*targetSection->code)[ref->offset - 1] = ref->patchCode2;
            }
          }
          char relocHigh = (relocValue >> 4) & 0xFF;
          char relocLow = relocValue & 0x0F;
          (*targetSection->code)[ref->offset] = ((*targetSection->code)[ref->offset] | relocLow); // 0x0X
          (*targetSection->code)[ref->offset + 1] = relocHigh;                                    // 0xXX
        }

        // UPIS VREDNOSTI U MEMORIJU
        if (!literalEntry->isSymbol)
        {
          unsigned long value = stoul(key, nullptr, 0);
          write_to_memory_data_long(currSection->idSymbolTable, locationCounter, value);
        }
        else
        {
          auto symIt = symbolTable->find(key);
          if (symIt == symbolTable->end())
          {
            cerr << "Greska: Simbol '" << key << "' nije pronadjen u tabeli simbola!" << endl;
            write_to_memory_data(currSection->idSymbolTable, locationCounter, 0);
            locationCounter += 4;
            continue;
          }

          SymbolTableEntry *symbol = symIt->second;

          if (symbol->value != -1 && symbol->ndx == currSection->idSymbolTable)
          {
            currSection->size -= 4;
            currSection->literalPoolSize -= 4;
            continue;
          }
          else
          {
            // Globalan ili simbol iz druge sekcije -> relokacija
            write_to_memory_data(currSection->idSymbolTable, locationCounter, 0);

            if (symbol->value != -1)
            {
              RelocationTableEntry *relocEntry = new RelocationTableEntry(
                  locationCounter,
                  1, // apsolutna
                  calculate_addend(1, symbol),
                  currSection->idSymbolTable,
                  calculate_reloc_idSymbol(symbol));

              currSection->relocationTableForSection->push_back(relocEntry);
            }
            else
            {
              // Dodaj u symbol->flink
              symbol->flink->push_back(new ForwardReferenceTableEntry(currSection->idSymbolTable, locationCounter, 1));
            }
          }
        }

        locationCounter += 4;
      }
    }

    currSection->size = locationCounter;
    literalPool->clear();
    literalInsertionOrder.clear();
  }
  //Kreiramo novu sekciju i dodajemo je u `symbolTable` i `allSections`
  if (symbolTable->find(*name) != symbolTable->end())
  {
    cerr << "Greska: Sekcija '" << *name << "' je vec definisana!" << endl;
    return;
  }

  // Dodajemo sekciju u tabelu simbola
  SymbolTableEntry *newSectionSymbol = new SymbolTableEntry(*name, 0, 0, 0, 0, true);
  symbolTable->insert({*name, newSectionSymbol});

  // Kreiramo novu sekciju
  Section *newSection = new Section(*name, newSectionSymbol->id);

  // Dodajemo je u `allSections`
  allSections->insert({newSectionSymbol->id, newSection});

  // Azuriramo `currSection` i resetujemo `locationCounter`
  currSection = newSection;
  locationCounter = 0;
}

void process_SKIP_DIR(string *val)
{
  if (!val || val->empty())
    return;

  int num = stoi(*val, nullptr, 0);

  if (num < 0)
  {
    cerr << "Greska: .skip ne moze imati negativan broj bajtova!" << endl;
    return;
  }

  if (!currSection || !currSection->code)
  {
    cerr << "Greska: .skip direktiva je koriscena pre nego sto je definisana sekcija!" << endl;
    return;
  }

  currSection->code->insert(currSection->code->end(), num, 0);

  locationCounter += num;
}

/* ===================================================================================================================== */

/* INSTRUKCIJE */

void process_HALT_INSTR()
{
  if (!currSection || !currSection->code)
  {
    cerr << "Greska: HALT instrukcija mora biti unutar sekcije!" << endl;
    return;
  }

  currSection->code->push_back(0x00);
  currSection->code->push_back(0x00);
  currSection->code->push_back(0x00);
  currSection->code->push_back(0x00);

  locationCounter += 4;

  if (locationCounter > 4096)
  {
    cout << "ERROR in HALT: Section shouldn't be larger than 4096B";
    exit(-1);
  }
}

void process_INT_INSTR()
{
  if (!currSection || !currSection->code)
  {
    cerr << "Greska: INT instrukcija mora biti unutar sekcije!" << endl;
    return;
  }

  // Opcode za INT instrukciju je 4 bajta -> 0x00000001
  currSection->code->push_back(0x10);
  currSection->code->push_back(0x00);
  currSection->code->push_back(0x00);
  currSection->code->push_back(0x00);

  locationCounter += 4;

  if (locationCounter > 4096)
  {
    cout << "ERROR in INT: Section shouldn't be larger than 4096B";
    exit(-1);
  }
}

void process_IRET_INSTR()
{
  /* ne moze preko pop-a jer ako se radi preko pop-a on ide sekv pa mora prvo pop pc,
    a ako tako uradimo onda se nikada necemo vratiti na pop status
    da bismo ovo zaobisli, radimo preko ldr instrurkcije i to prvo ldr %status, [%sp + 4]; ldr %pc, [%sp]; %sp+=8;
  */
  if (!currSection || !currSection->code)
  {
    cerr << "Greska: IRET instrukcija mora biti unutar sekcije!" << endl;
    return;
  }
  // ldr %status, [%sp + 4];
  currSection->code->push_back(0x96);
  currSection->code->push_back(0x0e);
  currSection->code->push_back(0x04);
  currSection->code->push_back(0x00);

  // ldr %pc, [%sp]; %sp+=8;
  currSection->code->push_back(0x93);
  currSection->code->push_back(0xfe);
  currSection->code->push_back(0x08);
  currSection->code->push_back(0x00);

  locationCounter += 8;

  if (locationCounter > 4096)
  {
    cout << "ERROR in IRET: Section shouldn't be larger than 4096B";
    exit(-1);
  }
}

void process_CALL_INSTR(Arguments *arg)
{
  if (!arg || !arg->argName || !arg->argType || arg->argName->empty())
  {
    cerr << "Greska: CALL instrukcija zahteva operand!" << endl;
    return;
  }

  if (!currSection || !currSection->code)
  {
    cerr << "Greska: CALL mora biti unutar sekcije!" << endl;
    return;
  }

  string operand = *arg->argName->at(0);
  int type = arg->argType->at(0);

  if (type == 0)
  { // LITERAL
    unsigned long value = 0;
    try
    {
      value = stoul(operand, nullptr, 0); // podrzava heks, okt, dec
    }
    catch (...)
    {
      cerr << "Greska: Nevalidna literal vrednost u CALL instrukciji!" << endl;
      return;
    }

    if (value >= 0 && value <= 0xFFF)
    {
      // Direktno u kod (12-bitni offset)
      currSection->code->push_back(0x20);
      currSection->code->push_back(0x00);
      currSection->code->push_back((value & 0x0F));
      currSection->code->push_back(((value & 0xFF0) >> 4));
    }
    else
    {
      // Ne moze da stane -> koristi bazen literala
      if (literalPool->find(operand) == literalPool->end())
      {
        (*literalPool)[operand] = new LiteralPoolEntry(currSection->idSymbolTable, locationCounter + 2, false);
        literalInsertionOrder.push_back(operand);
      }
      else
      {
        (*literalPool)[operand]->flink->push_back(new ForwardReferenceTableEntry(currSection->idSymbolTable, locationCounter + 2, 0));
      }

      // Ugradi genericki call koji ce biti patch-ovan u .end
      currSection->code->push_back(0x21); // ldr pc, [pc + offset]
      currSection->code->push_back(0xf0);
      currSection->code->push_back(0x00); // offset ce se patchovati
      currSection->code->push_back(0x00);
    }
  }
  else if (type == 1)
  { // SIMBOL
    auto it = symbolTable->find(operand);
    if (it != symbolTable->end() && it->second->value != -1)
    {
      SymbolTableEntry *symbol = it->second;

      if (symbol->ndx == currSection->idSymbolTable)
      {
        int offset = symbol->value - locationCounter - 4;
        currSection->code->push_back(0x20);
        currSection->code->push_back(0xf0);
        currSection->code->push_back((offset & 0x0F));
        currSection->code->push_back(((offset & 0xFF0) >> 4));
      }
      else
      {
        // dodaj u bazen literala, a relokacija ce se po potrebi praviti u .section/.end
        if (literalPool->find(operand) == literalPool->end())
        {
          (*literalPool)[operand] = new LiteralPoolEntry(currSection->idSymbolTable, locationCounter + 2, true);
          (*literalPool)[operand]->isSymbol = true;
          literalInsertionOrder.push_back(operand);
        }
        else
        {
          (*literalPool)[operand]->flink->push_back(new ForwardReferenceTableEntry(currSection->idSymbolTable, locationCounter + 2, 0));
        }

        // Ugradi instrukciju koja ce biti patchovana
        currSection->code->push_back(0x21); // ldr pc, [pc + offset]
        currSection->code->push_back(0xf0);
        currSection->code->push_back(0x00);
        currSection->code->push_back(0x00);
      }
    }
    else
    {
      // Simbol nije jos definisan
      if (symbolTable->find(operand) == symbolTable->end())
      {
        symbolTable->insert({operand, new SymbolTableEntry(operand, 2, 1, 0, -1, false)});
      }

      ForwardReferenceTableEntry *helper = nullptr;

      if (literalPool->find(operand) == literalPool->end())
      {
        (*literalPool)[operand] = new LiteralPoolEntry(currSection->idSymbolTable, locationCounter + 2, true);
        // FALI I OVDE HELPER
        helper = (*literalPool)[operand]->flink->at(0);
        (*literalPool)[operand]->isSymbol = true;
        literalInsertionOrder.push_back(operand);
      }
      else
      {
        helper = new ForwardReferenceTableEntry(currSection->idSymbolTable, locationCounter + 2, 0);
        (*literalPool)[operand]->flink->push_back(helper);
      }

      helper->setPatchOpcode(0x20);

      // Placeholder instrukcija
      currSection->code->push_back(0x21); // ldr pc, [pc + offset]
      currSection->code->push_back(0xf0);
      currSection->code->push_back(0x00);
      currSection->code->push_back(0x00);
    }
  }

  locationCounter += 4;

  if (locationCounter > 4096)
  {
    cerr << "ERROR: CALL prekoracuje maksimalnu velicinu sekcije!" << endl;
    exit(-1);
  }
}

void process_RET_INSTR()
{
  if (!currSection || !currSection->code)
  {
    cerr << "Greska: RET instrukcija mora biti unutar sekcije!" << endl;
    return;
  }
  currSection->code->push_back(0x93);
  currSection->code->push_back(0xfe);
  currSection->code->push_back(0x04);
  currSection->code->push_back(0x00);

  locationCounter += 4;

  if (locationCounter > 4096)
  {
    cout << "ERROR in RET: Section shouldn't be larger than 4096B";
    exit(-1);
  }
}

void process_JMP_INSTR(Arguments *arg)
{
  if (!arg || !arg->argName || !arg->argType || arg->argName->empty())
  {
    cerr << "Greska: JMP instrukcija zahteva operand!" << endl;
    return;
  }

  if (!currSection || !currSection->code)
  {
    cerr << "Greska: JMP mora biti unutar sekcije!" << endl;
    return;
  }

  string operand = *arg->argName->at(0);
  int type = arg->argType->at(0);

  if (type == 0)
  { // LITERAL
    unsigned long value = 0;
    try
    {
      value = stoul(operand, nullptr, 0); // podrzava heks, okt, dec
    }
    catch (...)
    {
      cerr << "Greska: Nevalidna literal vrednost u JMP instrukciji!" << endl;
      return;
    }

    if (value >= 0 && value <= 0xFFF)
    {
      // Direktno u kod (12-bitni offset)
      currSection->code->push_back(0x30);
      currSection->code->push_back(0x00);
      currSection->code->push_back((value & 0x0F));
      currSection->code->push_back(((value & 0xFF0) >> 4));
    }
    else
    {
      // Ne moze da stane -> koristi bazen literala
      if (literalPool->find(operand) == literalPool->end())
      {
        (*literalPool)[operand] = new LiteralPoolEntry(currSection->idSymbolTable, locationCounter + 2, false);
        literalInsertionOrder.push_back(operand);
      }
      else
      {
        (*literalPool)[operand]->flink->push_back(new ForwardReferenceTableEntry(currSection->idSymbolTable, locationCounter + 2, 0));
      }

      // Ugradi genericki jmp koji ce biti patch-ovan u .end
      currSection->code->push_back(0x38); // ldr pc, [pc + offset]
      currSection->code->push_back(0xf0);
      currSection->code->push_back(0x00); // offset ce se patchovati
      currSection->code->push_back(0x00);
    }
  }
  else if (type == 1)
  { // SIMBOL
    auto it = symbolTable->find(operand);
    if (it != symbolTable->end() && it->second->value != -1)
    {
      SymbolTableEntry *symbol = it->second;

      if (symbol->ndx == currSection->idSymbolTable)
      {
        int offset = symbol->value - locationCounter - 4;
        currSection->code->push_back(0x30);
        currSection->code->push_back(0xf0);
        currSection->code->push_back((offset & 0x0F));
        currSection->code->push_back(((offset & 0xFF0) >> 4));
      }
      else
      {
        // dodaj u bazen literala, a relokacija ce se po potrebi praviti u .section/.end
        if (literalPool->find(operand) == literalPool->end())
        {
          (*literalPool)[operand] = new LiteralPoolEntry(currSection->idSymbolTable, locationCounter + 2, true);
          (*literalPool)[operand]->isSymbol = true;
          literalInsertionOrder.push_back(operand);
        }
        else
        {
          (*literalPool)[operand]->flink->push_back(new ForwardReferenceTableEntry(currSection->idSymbolTable, locationCounter + 2, 0));
        }

        // Ugradi instrukciju koja ce biti patchovana
        currSection->code->push_back(0x38); // ldr pc, [pc + offset]
        currSection->code->push_back(0xf0);
        currSection->code->push_back(0x00);
        currSection->code->push_back(0x00);
      }
    }
    else
    {
      // Simbol nije jos definisan
      if (symbolTable->find(operand) == symbolTable->end())
      {
        symbolTable->insert({operand, new SymbolTableEntry(operand, 2, 1, 0, -1, false)});
      }

      ForwardReferenceTableEntry *helper = nullptr;

      if (literalPool->find(operand) == literalPool->end())
      {
        (*literalPool)[operand] = new LiteralPoolEntry(currSection->idSymbolTable, locationCounter + 2, true);
        helper = (*literalPool)[operand]->flink->at(0);
        (*literalPool)[operand]->isSymbol = true;
        literalInsertionOrder.push_back(operand);
      }
      else
      {
        helper = new ForwardReferenceTableEntry(currSection->idSymbolTable, locationCounter + 2, 0);
        (*literalPool)[operand]->flink->push_back(helper);
      }

      helper->setPatchOpcode(0x30);

      // Placeholder instrukcija
      currSection->code->push_back(0x38); // ldr pc, [pc + offset]
      currSection->code->push_back(0xf0);
      currSection->code->push_back(0x00);
      currSection->code->push_back(0x00);
    }
  }

  locationCounter += 4;

  if (locationCounter > 4096)
  {
    cerr << "ERROR: JMP prekoracuje maksimalnu velicinu sekcije!" << endl;
    exit(-1);
  }
}

void process_BEQ_INSTR(string *gpr1, string *gpr2, Arguments *arg)
{
  if (!arg || !arg->argName || !arg->argType || arg->argName->empty())
  {
    cerr << "Greska: BEQ instrukcija zahteva operand!" << endl;
    return;
  }

  if (!currSection || !currSection->code)
  {
    cerr << "Greska: BEQ mora biti unutar sekcije!" << endl;
    return;
  }
  int gpr1Num = stoi(gpr1->substr(1)); // gpr1 je b
  int gpr2Num = stoi(gpr2->substr(1)); // gpr2 je c
  string operand = *arg->argName->at(0);
  int type = arg->argType->at(0);

  if (type == 0)
  { // LITERAL
    unsigned long value = 0;
    try
    {
      value = stoul(operand, nullptr, 0); // podrzava heks, okt, dec
    }
    catch (...)
    {
      cerr << "Greska: Nevalidna literal vrednost u BEQ instrukciji!" << endl;
      return;
    }

    if (value >= 0 && value <= 0xFFF)
    {
      // Direktno u kod (12-bitni offset)
      currSection->code->push_back(0x31);
      currSection->code->push_back((0x00 | gpr1Num));
      currSection->code->push_back(((gpr2Num << 4) | (value & 0x0F)));
      currSection->code->push_back(((value >> 4) & 0xFF));
    }
    else
    {
      // Ne moze da stane-> koristi bazen literala
      if (literalPool->find(operand) == literalPool->end())
      {
        (*literalPool)[operand] = new LiteralPoolEntry(currSection->idSymbolTable, locationCounter + 2, false);
        literalInsertionOrder.push_back(operand);
      }
      else
      {
        (*literalPool)[operand]->flink->push_back(new ForwardReferenceTableEntry(currSection->idSymbolTable, locationCounter + 2, 0));
      }

      // Ugradi genericki beq koji ce biti patch-ovan u .end
      currSection->code->push_back(0x39);
      currSection->code->push_back(((0xF << 4) | (gpr1Num & 0x0F)));
      currSection->code->push_back(((gpr2Num << 4) | 0x00));
      currSection->code->push_back(0x00);
    }
  }
  else if (type == 1)
  { // SIMBOL
    auto it = symbolTable->find(operand);
    if (it != symbolTable->end() && it->second->value != -1)
    {
      SymbolTableEntry *symbol = it->second;

      if (symbol->ndx == currSection->idSymbolTable)
      {
        int offset = symbol->value - locationCounter - 4;
        currSection->code->push_back(0x31);
        currSection->code->push_back(((0xF << 4) | (gpr1Num & 0x0F)));
        currSection->code->push_back(((gpr2Num << 4) | (offset & 0x0F)));
        currSection->code->push_back(((offset & 0xFF0) >> 4));
      }
      else
      {
        // dodaj u bazen literala, a relokacija ce se po potrebi praviti u .section/.end
        if (literalPool->find(operand) == literalPool->end())
        {
          (*literalPool)[operand] = new LiteralPoolEntry(currSection->idSymbolTable, locationCounter + 2, true);
          (*literalPool)[operand]->isSymbol = true;
          literalInsertionOrder.push_back(operand);
        }
        else
        {
          (*literalPool)[operand]->flink->push_back(new ForwardReferenceTableEntry(currSection->idSymbolTable, locationCounter + 2, 0));
        }

        // Ugradi instrukciju koja ce biti patchovana
        currSection->code->push_back(0x39); // ldr pc, [pc + offset]
        currSection->code->push_back(((0xF << 4) | (gpr1Num & 0x0F)));
        currSection->code->push_back(((gpr2Num << 4) | 0x00));
        currSection->code->push_back(0x00);
      }
    }
    else
    {
      // Simbol nije jos definisan
      if (symbolTable->find(operand) == symbolTable->end())
      {
        symbolTable->insert({operand, new SymbolTableEntry(operand, 2, 1, 0, -1, false)});
      }

      ForwardReferenceTableEntry *helper = nullptr;

      if (literalPool->find(operand) == literalPool->end())
      {
        (*literalPool)[operand] = new LiteralPoolEntry(currSection->idSymbolTable, locationCounter + 2, true);
        helper = (*literalPool)[operand]->flink->at(0);
        (*literalPool)[operand]->isSymbol = true;
        literalInsertionOrder.push_back(operand);
      }
      else
      {
        helper = new ForwardReferenceTableEntry(currSection->idSymbolTable, locationCounter + 2, 0);
        (*literalPool)[operand]->flink->push_back(helper);
      }

      helper->setPatchOpcode(0x31);

      // Placeholder instrukcija
      currSection->code->push_back(0x39); // ldr pc, [pc + offset]
      currSection->code->push_back(((0xF << 4) | (gpr1Num & 0x0F)));
      currSection->code->push_back(((gpr2Num << 4) | 0x00));
      currSection->code->push_back(0x00);
    }
  }

  locationCounter += 4;

  if (locationCounter > 4096)
  {
    cerr << "ERROR: BEQ prekoracuje maksimalnu velicinu sekcije!" << endl;
    exit(-1);
  }
}

void process_BNE_INSTR(string *gpr1, string *gpr2, Arguments *arg)
{
  if (!arg || !arg->argName || !arg->argType || arg->argName->empty())
  {
    cerr << "Greska: BNE instrukcija zahteva operand!" << endl;
    return;
  }

  if (!currSection || !currSection->code)
  {
    cerr << "Greska: BNE mora biti unutar sekcije!" << endl;
    return;
  }
  int gpr1Num = stoi(gpr1->substr(1)); // gpr1 je b
  int gpr2Num = stoi(gpr2->substr(1)); // gpr2 je c
  string operand = *arg->argName->at(0);
  int type = arg->argType->at(0);

  if (type == 0)
  { // LITERAL
    unsigned long value = 0;
    try
    {
      value = stoul(operand, nullptr, 0); // podrzava heks, okt, dec
    }
    catch (...)
    {
      cerr << "Greska: Nevalidna literal vrednost u BNE instrukciji!" << endl;
      return;
    }

    if (value >= 0 && value <= 0xFFF)
    {
      // Direktno u kod (12-bitni offset)
      currSection->code->push_back(0x32);
      currSection->code->push_back((0x00 | gpr1Num));
      currSection->code->push_back(((gpr2Num << 4) | (value & 0x0F)));
      currSection->code->push_back(((value >> 4) & 0xFF));
    }
    else
    {
      // Ne moze da stane -> koristi bazen literala
      if (literalPool->find(operand) == literalPool->end())
      {
        (*literalPool)[operand] = new LiteralPoolEntry(currSection->idSymbolTable, locationCounter + 2, false);
        literalInsertionOrder.push_back(operand);
      }
      else
      {
        (*literalPool)[operand]->flink->push_back(new ForwardReferenceTableEntry(currSection->idSymbolTable, locationCounter + 2, 0));
      }

      currSection->code->push_back(0x3a);
      currSection->code->push_back(((0xF << 4) | (gpr1Num & 0x0F)));
      currSection->code->push_back(((gpr2Num << 4) | 0x00));
      currSection->code->push_back(0x00);
    }
  }
  else if (type == 1)
  { // SIMBOL
    auto it = symbolTable->find(operand);
    if (it != symbolTable->end() && it->second->value != -1)
    {
      SymbolTableEntry *symbol = it->second;

      if (symbol->ndx == currSection->idSymbolTable)
      {
        int offset = symbol->value - locationCounter - 4;
        currSection->code->push_back(0x32);
        currSection->code->push_back(((0xF << 4) | (gpr1Num & 0x0F)));
        currSection->code->push_back(((gpr2Num << 4) | (offset & 0x0F)));
        currSection->code->push_back(((offset & 0xFF0) >> 4));
      }
      else
      {
        // dodaj u bazen literala, a relokacija ce se po potrebi praviti u .section/.end
        if (literalPool->find(operand) == literalPool->end())
        {
          (*literalPool)[operand] = new LiteralPoolEntry(currSection->idSymbolTable, locationCounter + 2, true);
          (*literalPool)[operand]->isSymbol = true;
          literalInsertionOrder.push_back(operand);
        }
        else
        {
          (*literalPool)[operand]->flink->push_back(new ForwardReferenceTableEntry(currSection->idSymbolTable, locationCounter + 2, 0));
        }

        // Ugradi instrukciju koja ce biti patchovana
        currSection->code->push_back(0x3a); // ldr pc, [pc + offset]
        currSection->code->push_back(((0xF << 4) | (gpr1Num & 0x0F)));
        currSection->code->push_back(((gpr2Num << 4) | 0x00));
        currSection->code->push_back(0x00);
      }
    }
    else
    {
      // Simbol nije jos definisan
      if (symbolTable->find(operand) == symbolTable->end())
      {
        symbolTable->insert({operand, new SymbolTableEntry(operand, 2, 1, 0, -1, false)});
      }

      ForwardReferenceTableEntry *helper = nullptr;

      if (literalPool->find(operand) == literalPool->end())
      {
        (*literalPool)[operand] = new LiteralPoolEntry(currSection->idSymbolTable, locationCounter + 2, true);
        helper = (*literalPool)[operand]->flink->at(0);
        (*literalPool)[operand]->isSymbol = true;
        literalInsertionOrder.push_back(operand);
      }
      else
      {
        helper = new ForwardReferenceTableEntry(currSection->idSymbolTable, locationCounter + 2, 0);
        (*literalPool)[operand]->flink->push_back(helper);
      }

      helper->setPatchOpcode(0x32);

      // Placeholder instrukcija
      currSection->code->push_back(0x3a); // ldr pc, [pc + offset]
      currSection->code->push_back(((0xF << 4) | (gpr1Num & 0x0F)));
      currSection->code->push_back(((gpr2Num << 4) | 0x00));
      currSection->code->push_back(0x00);
    }
  }

  locationCounter += 4;

  if (locationCounter > 4096)
  {
    cerr << "ERROR: BNE prekoracuje maksimalnu velicinu sekcije!" << endl;
    exit(-1);
  }
}

void process_BGT_INSTR(string *gpr1, string *gpr2, Arguments *arg)
{
  if (!arg || !arg->argName || !arg->argType || arg->argName->empty())
  {
    cerr << "Greska: BGT instrukcija zahteva operand!" << endl;
    return;
  }

  if (!currSection || !currSection->code)
  {
    cerr << "Greska: BGT mora biti unutar sekcije!" << endl;
    return;
  }
  int gpr1Num = stoi(gpr1->substr(1)); // gpr1 je b
  int gpr2Num = stoi(gpr2->substr(1)); // gpr2 je c
  string operand = *arg->argName->at(0);
  int type = arg->argType->at(0);

  if (type == 0)
  { // LITERAL
    unsigned long value = 0;
    try
    {
      value = stoul(operand, nullptr, 0); // podrzava heks, okt, dec
    }
    catch (...)
    {
      cerr << "Greska: Nevalidna literal vrednost u BGT instrukciji!" << endl;
      return;
    }

    if (value >= 0 && value <= 0xFFF)
    {
      // Direktno u kod (12-bitni offset)
      currSection->code->push_back(0x33);
      currSection->code->push_back((0x00 | gpr1Num));
      currSection->code->push_back(((gpr2Num << 4) | (value & 0x0F)));
      currSection->code->push_back(((value >> 4) & 0xFF));
    }
    else
    {
      // Ne moze da stane -> koristi bazen literala
      if (literalPool->find(operand) == literalPool->end())
      {
        (*literalPool)[operand] = new LiteralPoolEntry(currSection->idSymbolTable, locationCounter + 2, false);
        literalInsertionOrder.push_back(operand);
      }
      else
      {
        (*literalPool)[operand]->flink->push_back(new ForwardReferenceTableEntry(currSection->idSymbolTable, locationCounter + 2, 0));
      }

      // Ugradi genericki call koji ce biti patch-ovan u .end
      currSection->code->push_back(0x3b);
      currSection->code->push_back(((0xF << 4) | (gpr1Num & 0x0F)));
      currSection->code->push_back(((gpr2Num << 4) | 0x00));
      currSection->code->push_back(0x00);
    }
  }
  else if (type == 1)
  { // SIMBOL
    auto it = symbolTable->find(operand);
    if (it != symbolTable->end() && it->second->value != -1)
    {
      SymbolTableEntry *symbol = it->second;

      if (symbol->ndx == currSection->idSymbolTable)
      {
        int offset = symbol->value - locationCounter - 4;
        currSection->code->push_back(0x33);
        currSection->code->push_back(((0xF << 4) | (gpr1Num & 0x0F)));
        currSection->code->push_back(((gpr2Num << 4) | (offset & 0x0F)));
        currSection->code->push_back(((offset & 0xFF0) >> 4));
      }
      else
      {
        // dodaj u bazen literala, a relokacija ce se po potrebi praviti u .section/.end
        if (literalPool->find(operand) == literalPool->end())
        {
          (*literalPool)[operand] = new LiteralPoolEntry(currSection->idSymbolTable, locationCounter + 2, true);
          (*literalPool)[operand]->isSymbol = true;
          literalInsertionOrder.push_back(operand);
        }
        else
        {
          (*literalPool)[operand]->flink->push_back(new ForwardReferenceTableEntry(currSection->idSymbolTable, locationCounter + 2, 0));
        }

        // Ugradi instrukciju koja ce biti patchovana
        currSection->code->push_back(0x3b); // ldr pc, [pc + offset]
        currSection->code->push_back(((0xF << 4) | (gpr1Num & 0x0F)));
        currSection->code->push_back(((gpr2Num << 4) | 0x00));
        currSection->code->push_back(0x00);
      }
    }
    else
    {
      // Simbol nije jos definisan
      if (symbolTable->find(operand) == symbolTable->end())
      {
        symbolTable->insert({operand, new SymbolTableEntry(operand, 2, 1, 0, -1, false)});
      }

      ForwardReferenceTableEntry *helper = nullptr;

      if (literalPool->find(operand) == literalPool->end())
      {
        (*literalPool)[operand] = new LiteralPoolEntry(currSection->idSymbolTable, locationCounter + 2, true);
        helper = (*literalPool)[operand]->flink->at(0);
        (*literalPool)[operand]->isSymbol = true;
        literalInsertionOrder.push_back(operand);
      }
      else
      {
        helper = new ForwardReferenceTableEntry(currSection->idSymbolTable, locationCounter + 2, 0);
        (*literalPool)[operand]->flink->push_back(helper);
      }

      helper->setPatchOpcode(0x33);

      // Placeholder instrukcija
      currSection->code->push_back(0x3b); // ldr pc, [pc + offset]
      currSection->code->push_back(((0xF << 4) | (gpr1Num & 0x0F)));
      currSection->code->push_back(((gpr2Num << 4) | 0x00));
      currSection->code->push_back(0x00);
    }
  }

  locationCounter += 4;

  if (locationCounter > 4096)
  {
    cerr << "ERROR: BGT prekoracuje maksimalnu velicinu sekcije!" << endl;
    exit(-1);
  }
}

void process_PUSH_INSTR(string *gpr)
{
  if (!currSection || !currSection->code)
  {
    cerr << "Greska: PUSH instrukcija mora biti unutar sekcije!" << endl;
    return;
  }
  int regNum = stoi(gpr->substr(1));
  // sp<=sp-4; mem32[sp] <= gpr; preko st MMMM=0b0001 gde D = -4
  currSection->code->push_back(0x81);
  currSection->code->push_back(0xe0);
  currSection->code->push_back((regNum << 4) | 0x0c);
  currSection->code->push_back(0xff);

  locationCounter += 4;

  if (locationCounter > 4096)
  {
    cout << "ERROR in PUSH: Section shouldn't be larger than 4096B";
    exit(-1);
  }
}

void process_POP_INSTR(string *gpr)
{
  if (!currSection || !currSection->code)
  {
    cerr << "Greska: POP instrukcija mora biti unutar sekcije!" << endl;
    return;
  }
  int regNum = stoi(gpr->substr(1));
  // gpr <= mem32[sp]; sp <= sp + 4;
  currSection->code->push_back(0x93);
  currSection->code->push_back((regNum << 4) | 0x0e);
  currSection->code->push_back(0x04);
  currSection->code->push_back(0x00);

  locationCounter += 4;

  if (locationCounter > 4096)
  {
    cout << "ERROR in POP: Section shouldn't be larger than 4096B";
    exit(-1);
  }
}

void process_XCHG_INSTR(string *gprS, string *gprD) // proveri code
{
  if (!currSection || !currSection->code)
  {
    cerr << "Greska: XCHG instrukcija mora biti unutar sekcije!" << endl;
    return;
  }

  int dNum = stoi(gprD->substr(1));
  int sNum = stoi(gprS->substr(1));
  // temp <= gprD; gprD <= gprS; gprS <= temp;

  currSection->code->push_back(0x40);
  currSection->code->push_back((dNum & 0x0F));      // gprD kao niska cifra
  currSection->code->push_back((sNum << 4) & 0xF0); // gprS kao visoka cifra
  currSection->code->push_back(0x00);

  locationCounter += 4;

  if (locationCounter > 4096)
  {
    cout << "ERROR in XCHG: Section shouldn't be larger than 4096B";
    exit(-1);
  }
}

void process_ADD_INSTR(string *gprS, string *gprD)
{
  if (!currSection || !currSection->code)
  {
    cerr << "Greska: ADD instrukcija mora biti unutar sekcije!" << endl;
    return;
  }
  // gprD <= gprD + gprS;
  int dNum = stoi(gprD->substr(1)); // gprD = "rX"
  int sNum = stoi(gprS->substr(1)); // gprS = "rY"
  currSection->code->push_back(0x50);
  currSection->code->push_back((dNum << 4) | dNum);
  currSection->code->push_back((sNum << 4) | 0x00);
  currSection->code->push_back(0x00);

  locationCounter += 4;

  if (locationCounter > 4096)
  {
    cout << "ERROR in ADD: Section shouldn't be larger than 4096B";
    exit(-1);
  }
}

void process_SUB_INSTR(string *gprS, string *gprD)
{
  if (!currSection || !currSection->code)
  {
    cerr << "Greska: SUB instrukcija mora biti unutar sekcije!" << endl;
    return;
  }

  // gprD <= gprD - gprS;
  int dNum = stoi(gprD->substr(1)); // gprD = "rX"
  int sNum = stoi(gprS->substr(1)); // gprS = "rY"
  currSection->code->push_back(0x51);
  currSection->code->push_back((dNum << 4) | dNum);
  currSection->code->push_back((sNum << 4) | 0x00);
  currSection->code->push_back(0x00);

  locationCounter += 4;

  if (locationCounter > 4096)
  {
    cout << "ERROR in SUB: Section shouldn't be larger than 4096B";
    exit(-1);
  }
}
void process_MUL_INSTR(string *gprS, string *gprD)
{
  if (!currSection || !currSection->code)
  {
    cerr << "Greska: MUL instrukcija mora biti unutar sekcije!" << endl;
    return;
  }

  // gprD <= gprD * gprS;
  int dNum = stoi(gprD->substr(1)); // gprD = "rX"
  int sNum = stoi(gprS->substr(1)); // gprS = "rY"
  currSection->code->push_back(0x52);
  currSection->code->push_back((dNum << 4) | dNum);
  currSection->code->push_back((sNum << 4) | 0x00);
  currSection->code->push_back(0x00);

  locationCounter += 4;

  if (locationCounter > 4096)
  {
    cout << "ERROR in MUL: Section shouldn't be larger than 4096B";
    exit(-1);
  }
}
void process_DIV_INSTR(string *gprS, string *gprD)
{
  if (!currSection || !currSection->code)
  {
    cerr << "Greska: DIV instrukcija mora biti unutar sekcije!" << endl;
    return;
  }

  // gprD <= gprD * gprS;
  int dNum = stoi(gprD->substr(1)); // gprD = "rX"
  int sNum = stoi(gprS->substr(1)); // gprS = "rY"
  currSection->code->push_back(0x53);
  currSection->code->push_back((dNum << 4) | dNum);
  currSection->code->push_back((sNum << 4) | 0x00);
  currSection->code->push_back(0x00);

  locationCounter += 4;

  if (locationCounter > 4096)
  {
    cout << "ERROR in DIV: Section shouldn't be larger than 4096B";
    exit(-1);
  }
}

void process_NOT_INSTR(string *gpr)
{
  if (!currSection || !currSection->code)
  {
    cerr << "Greska: NOT instrukcija mora biti unutar sekcije!" << endl;
    return;
  }

  // gprD <= gprD * gprS;
  int regNum = stoi(gpr->substr(1)); // gprD = "rX"
  currSection->code->push_back(0x60);
  currSection->code->push_back((regNum << 4) | regNum);
  currSection->code->push_back(0x00);
  currSection->code->push_back(0x00);

  locationCounter += 4;

  if (locationCounter > 4096)
  {
    cout << "ERROR in NOT: Section shouldn't be larger than 4096B";
    exit(-1);
  }
}

void process_AND_INSTR(string *gprS, string *gprD)
{
  if (!currSection || !currSection->code)
  {
    cerr << "Greska: AND instrukcija mora biti unutar sekcije!" << endl;
    return;
  }

  // gprD <= gprD * gprS;
  int dNum = stoi(gprD->substr(1)); // gprD = "rX"
  int sNum = stoi(gprS->substr(1)); // gprS = "rY"
  currSection->code->push_back(0x61);
  currSection->code->push_back((dNum << 4) | dNum);
  currSection->code->push_back((sNum << 4) | 0x00);
  currSection->code->push_back(0x00);

  locationCounter += 4;

  if (locationCounter > 4096)
  {
    cout << "ERROR in AND: Section shouldn't be larger than 4096B";
    exit(-1);
  }
}
void process_OR_INSTR(string *gprS, string *gprD)
{
  if (!currSection || !currSection->code)
  {
    cerr << "Greska: OR instrukcija mora biti unutar sekcije!" << endl;
    return;
  }

  // gprD <= gprD * gprS;
  int dNum = stoi(gprD->substr(1)); // gprD = "rX"
  int sNum = stoi(gprS->substr(1)); // gprS = "rY"
  currSection->code->push_back(0x62);
  currSection->code->push_back((dNum << 4) | dNum);
  currSection->code->push_back((sNum << 4) | 0x00);
  currSection->code->push_back(0x00);

  locationCounter += 4;

  if (locationCounter > 4096)
  {
    cout << "ERROR in OR: Section shouldn't be larger than 4096B";
    exit(-1);
  }
}

void process_XOR_INSTR(string *gprS, string *gprD)
{
  if (!currSection || !currSection->code)
  {
    cerr << "Greska: XOR instrukcija mora biti unutar sekcije!" << endl;
    return;
  }

  // gprD <= gprD * gprS;
  int dNum = stoi(gprD->substr(1)); // gprD = "rX"
  int sNum = stoi(gprS->substr(1)); // gprS = "rY"
  currSection->code->push_back(0x63);
  currSection->code->push_back((dNum << 4) | dNum);
  currSection->code->push_back((sNum << 4) | 0x00);
  currSection->code->push_back(0x00);

  locationCounter += 4;

  if (locationCounter > 4096)
  {
    cout << "ERROR in XOR: Section shouldn't be larger than 4096B";
    exit(-1);
  }
}

void process_SHL_INSTR(string *gprS, string *gprD)
{
  if (!currSection || !currSection->code)
  {
    cerr << "Greska: SHL instrukcija mora biti unutar sekcije!" << endl;
    return;
  }

  // gprD <= gprD * gprS;
  int dNum = stoi(gprD->substr(1)); // gprD = "rX"
  int sNum = stoi(gprS->substr(1)); // gprS = "rY"
  currSection->code->push_back(0x70);
  currSection->code->push_back((dNum << 4) | dNum);
  currSection->code->push_back((sNum << 4) | 0x00);
  currSection->code->push_back(0x00);

  locationCounter += 4;

  if (locationCounter > 4096)
  {
    cout << "ERROR in SHL: Section shouldn't be larger than 4096B";
    exit(-1);
  }
}

void process_SHR_INSTR(string *gprS, string *gprD)
{
  if (!currSection || !currSection->code)
  {
    cerr << "Greska: SHR instrukcija mora biti unutar sekcije!" << endl;
    return;
  }

  // gprD <= gprD * gprS;
  int dNum = stoi(gprD->substr(1)); // gprD = "rX"
  int sNum = stoi(gprS->substr(1)); // gprS = "rY"
  currSection->code->push_back(0x71);
  currSection->code->push_back((dNum << 4) | dNum);
  currSection->code->push_back((sNum << 4) | 0x00);
  currSection->code->push_back(0x00);

  locationCounter += 4;

  if (locationCounter > 4096)
  {
    cout << "ERROR in SHR: Section shouldn't be larger than 4096B";
    exit(-1);
  }
}
void process_LD_INSTR(Arguments *arg, string *gpr)
{
  if (!arg || !arg->argName || arg->argName->empty() || !gpr)
  {
    cerr << "Greska: LD instrukcija zahteva operand i odredisni registar!" << endl;
    return;
  }

  if (!currSection || !currSection->code)
  {
    cerr << "Greska: LD mora biti unutar sekcije!" << endl;
    return;
  }
  int dstReg = 0;
  try
  {
    dstReg = stoi(gpr->substr(1, 2), nullptr, 0);
  }
  catch (...)
  {
    cerr << "Nevalidan dstReg u LD instrukciji" << endl;
    exit(-1);
  }

  string operand = *arg->argName->at(0);
  int type = arg->argType->at(0);
  // int dstReg = stoi(gpr->substr(1)); // GPR A

  switch (arg->addrMode)
  {
  case IMMEDIATE_LITERAL: // ok
  {
    unsigned long value = 0;
    try
    {
      value = stoul(operand, nullptr, 0);
    }
    catch (...)
    {
      cerr << "Greska: Nevalidan literal '" << operand << "' u LD instrukciji!" << endl;
      return;
    }

    if (value >= 0 && value <= 0xFFF)
    {
      // Moze da stane u 12 bita -> direktno kodiranje u instrukciju
      currSection->code->push_back(0x91);
      currSection->code->push_back((dstReg << 4) | 0x0);
      currSection->code->push_back((value & 0x0F));
      currSection->code->push_back((value & 0xFF0) >> 4);
    }
    else
    {
      // Ne moze da stane -> koristi literal pool
      if (literalPool->find(operand) == literalPool->end())
      {
        (*literalPool)[operand] = new LiteralPoolEntry(currSection->idSymbolTable, locationCounter + 2, false);
        literalInsertionOrder.push_back(operand);
      }
      else
      {
        (*literalPool)[operand]->flink->push_back(
            new ForwardReferenceTableEntry(currSection->idSymbolTable, locationCounter + 2, 0));
      }
      // Ugradi indirektno ucitavanje sa PC-relative offsetom
      currSection->code->push_back(0x92);                 // gpr[A]<=mem32[gpr[B]+gpr[C]+D];
      currSection->code->push_back((dstReg << 4) | 0x0F); // B = PC
      currSection->code->push_back(0x00);
      currSection->code->push_back(0x00);
    }
    break;
  }

  case IMMEDIATE_SYMBOL: // ok
  {
    auto it = symbolTable->find(operand);
    if (it != symbolTable->end() && it->second->value != -1)
    {
      SymbolTableEntry *symbol = it->second;

      if (symbol->ndx == currSection->idSymbolTable)
      {
        // Simbol definisan u istoj sekciji -> koristi PC-relativni offset
        int offset = symbol->value - locationCounter - 4;
        currSection->code->push_back(0x91);                 // gpr[A] <= gpr[B] + D
        currSection->code->push_back((dstReg << 4) | 0x0F); // B = PC (regF)
        currSection->code->push_back(offset & 0x0F);
        currSection->code->push_back((offset >> 4) & 0xFF);
      }
      else
      {
        // Simbol definisan, ali u drugoj sekciji -> koristi literal pool
        if (literalPool->find(operand) == literalPool->end())
        {
          (*literalPool)[operand] = new LiteralPoolEntry(currSection->idSymbolTable, locationCounter + 2, true);
          literalInsertionOrder.push_back(operand);
        }
        else
        {
          (*literalPool)[operand]->flink->push_back(
              new ForwardReferenceTableEntry(currSection->idSymbolTable, locationCounter + 2, 0));
        }
        // Ugradi indirektno ucitavanje iz mem32[PC + D]
        currSection->code->push_back(0x92);                 // gpr[A] <= mem32[gpr[B]+gpr[C]+D]
        currSection->code->push_back((dstReg << 4) | 0x0F); // B = PC
        currSection->code->push_back(0x00);                 // C = 0, D low
        currSection->code->push_back(0x00);                 // D high
      }
    }
    else
    {
      // Simbol jos nije definisan
      if (symbolTable->find(operand) == symbolTable->end())
      {
        symbolTable->insert({operand, new SymbolTableEntry(operand, 2, 1, 0, -1, false)});
      }

      ForwardReferenceTableEntry *helper = nullptr;

      if (literalPool->find(operand) == literalPool->end())
      {
        (*literalPool)[operand] = new LiteralPoolEntry(currSection->idSymbolTable, locationCounter + 2, true);
        helper = (*literalPool)[operand]->flink->at(0);
        literalInsertionOrder.push_back(operand);
      }
      else
      {
        helper = new ForwardReferenceTableEntry(currSection->idSymbolTable, locationCounter + 2, 0);
        (*literalPool)[operand]->flink->push_back(helper);
      }

      helper->setPatchOpcode(0x91);

      // Placeholder instrukcija -> bice patchovana u .end
      currSection->code->push_back(0x92);
      currSection->code->push_back((dstReg << 4) | 0x0F);
      currSection->code->push_back(0x00);
      currSection->code->push_back(0x00);
    }

    break;
  }

  case MEMORY_DIRECT_LITERAL: // proveri
  {
    if (!arg || !arg->argName || arg->argName->empty())
    {
      cerr << "Greska: Neispravan operand za MEMORY_DIRECT_LITERAL!" << endl;
      return;
    }

    unsigned long literalValue;
    try
    {
      literalValue = stoul(operand, nullptr, 0);
    }
    catch (...)
    {
      cerr << "Greska: Neispravan literal u MEMORY_DIRECT_LITERAL!" << endl;
      return;
    }

    // Ako moze da stane u 12 bita, koristi jednu instrukciju (gpr <= mem32[PC + offset])
    if (literalValue >= 0 && literalValue <= 0xFFF)
    {
      currSection->code->push_back(0x92);
      currSection->code->push_back((dstReg << 4) | 0x00);
      currSection->code->push_back((literalValue & 0x0F));
      currSection->code->push_back(((literalValue & 0xFF0) >> 4));
    }
    else
    {
      //Ne moze da stane -> koristi bazen literala + dve instrukcije
      string key = operand;

      // Dodaj u bazen ako ne postoji
      if (literalPool->find(key) == literalPool->end())
      {
        (*literalPool)[key] = new LiteralPoolEntry(currSection->idSymbolTable, locationCounter + 2, false);
        literalInsertionOrder.push_back(key);
      }
      else
      {
        (*literalPool)[key]->flink->push_back(new ForwardReferenceTableEntry(currSection->idSymbolTable, locationCounter + 2, 0));
      }

      // Instrukcija 1: dstReg <literal>= mem32[pc + offset] (ucitava adresu tj literal iz bazena)
      currSection->code->push_back(0x92);
      currSection->code->push_back((dstReg << 4) | 0x0F);
      currSection->code->push_back(0x00);
      currSection->code->push_back(0x00); // D -> patch later

      // Instrukcija 2: dstReg <data>= mem32[literal]
      currSection->code->push_back(0x92);
      currSection->code->push_back((dstReg << 4) | dstReg);
      currSection->code->push_back(0x00); // D = 0, ne treba patch
      currSection->code->push_back(0x00);
    }

    if (literalValue > 0xFFF)
      locationCounter += 4; // druga instrukcija ako se koristi bazen

    break;
  }

  case MEMORY_DIRECT_SYMBOL: // proveri
  {

    auto it = symbolTable->find(operand);
    if (it != symbolTable->end() && it->second->value != -1)
    {
      SymbolTableEntry *symbol = it->second;

      if (symbol->ndx == currSection->idSymbolTable)
      {
        // Simbol u istoj sekciji -> moze PC-relativno
        int offset = symbol->value - locationCounter - 4;

        // Instrukcija 1: ucitavamo vrednost simbola u registar
        currSection->code->push_back(0x91);
        currSection->code->push_back(((dstReg << 4) | 0x0f));
        currSection->code->push_back((offset & 0x0f));
        currSection->code->push_back(((offset & 0xff0) >> 4));

        // Instrukcija 2: ucitavamo sadrzaj sa te adrese u registar
        currSection->code->push_back(0x92);
        currSection->code->push_back(((dstReg << 4) | dstReg));
        currSection->code->push_back(0x00);
        currSection->code->push_back(0x00);
      }
      else
      {
        // Simbol iz druge sekcije -> literal pool + patch
        if (literalPool->find(operand) == literalPool->end())
        {
          (*literalPool)[operand] = new LiteralPoolEntry(currSection->idSymbolTable, locationCounter + 2, true);
          (*literalPool)[operand]->isSymbol = true;
          literalInsertionOrder.push_back(operand);
        }
        else
        {
          (*literalPool)[operand]->flink->push_back(new ForwardReferenceTableEntry(currSection->idSymbolTable, locationCounter + 2, 0));
        }

        // Instrukcija 1: ucitavamo simbol iz bazena
        currSection->code->push_back(0x92);
        currSection->code->push_back(((dstReg << 4) | 0x0f));
        currSection->code->push_back(0x00); // to patch
        currSection->code->push_back(0x00);

        // Instrukcija 2: ucitavamo sadrzaj sa te adrese u registar
        currSection->code->push_back(0x92);
        currSection->code->push_back(((dstReg << 4) | dstReg));
        currSection->code->push_back(0x00);
        currSection->code->push_back(0x00);
      }
    }
    else
    {
      // Simbol nije jos poznat -> dodaj ga
      if (symbolTable->find(operand) == symbolTable->end())
      {
        symbolTable->insert({operand, new SymbolTableEntry(operand, 2, 1, 0, -1, false)});
      }

      ForwardReferenceTableEntry *helper = nullptr;

      if (literalPool->find(operand) == literalPool->end())
      {
        (*literalPool)[operand] = new LiteralPoolEntry(currSection->idSymbolTable, locationCounter + 2, true);
        helper = (*literalPool)[operand]->flink->at(0);
        (*literalPool)[operand]->isSymbol = true;
        literalInsertionOrder.push_back(operand);
      }
      else
      {
        helper = new ForwardReferenceTableEntry(currSection->idSymbolTable, locationCounter + 2, 0);
        (*literalPool)[operand]->flink->push_back(helper);
      }

      helper->setPatchOpcode(0x91);

      // Instrukcija 1: ucitavamo simbol iz bazena
      currSection->code->push_back(0x92);
      currSection->code->push_back(((dstReg << 4) | 0x0f));
      currSection->code->push_back(0x00); // to patch
      currSection->code->push_back(0x00);

      // Instrukcija 2: ucitavamo sadrzaj sa te adrese u registar
      currSection->code->push_back(0x92);
      currSection->code->push_back(((dstReg << 4) | dstReg));
      currSection->code->push_back(0x00);
      currSection->code->push_back(0x00);
    }

    locationCounter += 4;
    break;
  }

  case REGISTER_DIRECT: // ok
  {
    int srcReg = stoi(operand.substr(1, 2), nullptr, 0);
    currSection->code->push_back(0x91);
    currSection->code->push_back((dstReg << 4) | srcReg);
    currSection->code->push_back(0x00);
    currSection->code->push_back(0x00);
    break;
  }

  case REGISTER_INDIRECT: // ok
  {
    int baseReg = stoi(operand.substr(1, 2), nullptr, 0); // [%reg]
    currSection->code->push_back(0x93);
    currSection->code->push_back((dstReg << 4) | baseReg);
    currSection->code->push_back(0x00);
    currSection->code->push_back(0x00);
    break;
  }

  case REGISTER_OFFSET_LITERAL: // ISPRAVI DA IMAS I SRCREG I LITERAL LEPO
  {
    string operand = *arg->argName->at(1);
    string regSrc = *arg->argName->at(0);
    int value = 0;
    int srcReg = 0;

    try
    {
      value = stoi(operand, nullptr, 0);
      srcReg = stoi(regSrc.substr(1));
    }
    catch (...)
    {
      cerr << "Greska: Nevalidan literal u REGISTER_OFFSET_LITERAL!" << endl;
      return;
    }

    if (value < 0 || value > 0xFFF)
    {
      cerr << "Greska: Literal u REGISTER_OFFSET_LITERAL mora stati u 12 bita (0 - 4095)!" << endl;
      exit(-1); // Ili `return;` ako ne zelis da prekidas program
    }

    // Generisi instrukciju direktno
    currSection->code->push_back(0x92);
    currSection->code->push_back((dstReg << 4) | srcReg);      // B = bazni registar
    currSection->code->push_back((0x0 << 4) | (value & 0x0F)); // C = 0, D (nizi 4 bita)
    currSection->code->push_back((value >> 4) & 0xFF);         // D (visih 8 bita)

    break;
  }

  case REGISTER_OFFSET_SYMBOL:
  {
    // OVO JE ZA .equ
    break;
  }

  default:
    cerr << "Greska: Nepoznat rezim adresiranja za LD instrukciju!" << endl;
    return;
  }

  locationCounter += 4;

  if (locationCounter > 4096)
  {
    cerr << "ERROR: LD prekoracuje maksimalnu velicinu sekcije!" << endl;
    exit(-1);
  }
}

void process_ST_INSTR(string *gpr, Arguments *arg)
{
  if (!arg || !arg->argName || !gpr)
  {
    cerr << "Greska: ST instrukcija zahteva operand i izvorni registar!" << endl;
    return;
  }

  if (!currSection || !currSection->code)
  {
    cerr << "Greska: ST mora biti unutar sekcije!" << endl;
    return;
  }

  int srcReg = 0;
  try
  {
    srcReg = stoi(gpr->substr(1, 2), nullptr, 0);
  }
  catch (...)
  {
    cerr << "Nevalidan dstReg u LD instrukciji" << endl;
    exit(-1);
  }

  string operand = *arg->argName->at(0);
  int type = arg->argType->at(0);
  AddressingMode mode = arg->addrMode;

  switch (mode)
  {
  case REGISTER_DIRECT: // prepravljano, proveri
  {
    int dstReg = stoi(operand.substr(1, 2), nullptr, 0);
    currSection->code->push_back(0x91);
    currSection->code->push_back(((dstReg << 4) | srcReg));
    currSection->code->push_back(0x00);
    currSection->code->push_back(0x00);
    break;
  }

  case MEMORY_DIRECT_LITERAL: // prepravljano, proveri
  {
    unsigned long value = 0;
    try
    {
      value = stoul(operand, nullptr, 0);
    }
    catch (...)
    {
      cerr << "Greska: Nevalidna literal vrednost u ST instrukciji!" << endl;
      return;
    }

    if (value >= 0 && value <= 0xFFF)
    {
      currSection->code->push_back(0x80);
      currSection->code->push_back(0x00);
      currSection->code->push_back((srcReg << 4) | (value & 0x0F));
      currSection->code->push_back((value >> 4) & 0xFF);
    }
    else
    {
      string key = operand;

      if (literalPool->find(key) == literalPool->end())
      {
        (*literalPool)[key] = new LiteralPoolEntry(currSection->idSymbolTable, locationCounter + 2, false);
        literalInsertionOrder.push_back(key);
      }
      else
      {
        (*literalPool)[key]->flink->push_back(
            new ForwardReferenceTableEntry(currSection->idSymbolTable, locationCounter + 2, 0));
      }

      currSection->code->push_back(0x82);
      currSection->code->push_back(0xf0);
      currSection->code->push_back(((srcReg << 4) | (0x00))); // to patch
      currSection->code->push_back(0x00);
    }

    break;
  }

  case MEMORY_DIRECT_SYMBOL: // prepravljeno, proveri
  {
    auto it = symbolTable->find(operand);
    if (it != symbolTable->end() && it->second->value != -1)
    {
      SymbolTableEntry *symbol = it->second;

      if (symbol->ndx == currSection->idSymbolTable)
      {
        // Simbol iz iste sekcije -> moze direktno da se ugradi kao PC-relative offset
        int offset = symbol->value - locationCounter - 4;

        currSection->code->push_back(0x80); // mem32[gpr[A] + gpr[B] + D] <= gpr[C]
        currSection->code->push_back(0xf0); // gpr[A]=0, gpr[B]=0
        currSection->code->push_back((srcReg << 4) | (offset & 0x0F));
        currSection->code->push_back((offset >> 4) & 0xFF);
      }
      else
      {
        // Simbol iz druge sekcije -> koristi literal pool i patch kasnije
        if (literalPool->find(operand) == literalPool->end())
        {
          (*literalPool)[operand] = new LiteralPoolEntry(currSection->idSymbolTable, locationCounter + 2, true);
          (*literalPool)[operand]->isSymbol = true;
          literalInsertionOrder.push_back(operand);
        }
        else
        {
          (*literalPool)[operand]->flink->push_back(
              new ForwardReferenceTableEntry(currSection->idSymbolTable, locationCounter + 2, 0));
        }

        currSection->code->push_back(0x82);                 // indirektno preko mem[mem[gpr[PC]+D]]
        currSection->code->push_back(0xf0);                 // gpr[A]=PC, gpr[B]=0
        currSection->code->push_back((srcReg << 4) | 0x00); // C = srcReg
        currSection->code->push_back(0x00);                 // offset -> patch later
      }
    }
    else
    {
      // Simbol nije jos definisan -> dodaj ga i koristi literal pool
      if (symbolTable->find(operand) == symbolTable->end())
      {
        symbolTable->insert({operand, new SymbolTableEntry(operand, 2, 1, 0, -1, false)});
      }

      ForwardReferenceTableEntry *helper = nullptr;

      if (literalPool->find(operand) == literalPool->end())
      {
        (*literalPool)[operand] = new LiteralPoolEntry(currSection->idSymbolTable, locationCounter + 2, true);
        helper = (*literalPool)[operand]->flink->at(0);
        (*literalPool)[operand]->isSymbol = true;
        literalInsertionOrder.push_back(operand);
      }
      else
      {
        helper = new ForwardReferenceTableEntry(currSection->idSymbolTable, locationCounter + 2, 0);
        (*literalPool)[operand]->flink->push_back(helper);
      }

      helper->setPatchOpcode(0x80);

      currSection->code->push_back(0x82);
      currSection->code->push_back(0xf0);
      currSection->code->push_back((srcReg << 4) | 0x00);
      currSection->code->push_back(0x00); // D -> patch
    }

    break;
  }

  case REGISTER_INDIRECT: // prepravljano, proveri
  {
    int dstReg = stoi(operand.substr(1, 2), nullptr, 0);
    currSection->code->push_back(0x80);
    currSection->code->push_back((dstReg << 4));
    currSection->code->push_back((srcReg << 4));
    currSection->code->push_back(0x00);
    break;
  }

  case REGISTER_OFFSET_LITERAL:
  {
    int baseReg = stoi(arg->argName->at(0)->substr(1));
    int offset = stoi(*arg->argName->at(1));

    if (offset < 0 || offset > 0xFFF)
    {
      cerr << "Greska: Literal offset mora biti 12-bitni u ST!" << endl;
      return;
    }

    currSection->code->push_back(0x80);
    currSection->code->push_back((baseReg << 4) | 0x00);
    currSection->code->push_back((srcReg << 4) | (offset & 0x0F));
    currSection->code->push_back((offset >> 4) & 0xFF);
    break;
  }

  case REGISTER_OFFSET_SYMBOL:
  {
    string regStr = *arg->argName->at(0);
    string symName = *arg->argName->at(1);

    int baseReg = stoi(regStr.substr(1));
    auto it = symbolTable->find(symName);

    if (it == symbolTable->end() || it->second->value == -1)
    {
      cerr << "Greska: Simbol '" << symName << "' nije poznat za ST instrukciju!" << endl;
      return;
    }

    int offset = it->second->value;

    if (offset < 0 || offset > 0xFFF)
    {
      cerr << "Greska: Offset za simbol '" << symName << "' ne moze da stane u 12 bita!" << endl;
      return;
    }

    currSection->code->push_back(0x80);
    currSection->code->push_back((baseReg << 4) | 0x00);
    currSection->code->push_back((srcReg << 4) | (offset & 0x0F));
    currSection->code->push_back((offset >> 4) & 0xFF);
    break;
  }

  case IMMEDIATE_LITERAL:
  case IMMEDIATE_SYMBOL:
  {
    cerr << "Greska: ST instrukcija ne podrzava neposredno adresiranje (IMMEDIATE)!" << endl;
    exit(-1);
  }

  default:
    cerr << "Greska: Nepoznat rezim adresiranja za ST instrukciju!" << endl;
    exit(-1);
  }

  locationCounter += 4;

  if (locationCounter > 4096)
  {
    cerr << "ERROR: ST prekoracuje maksimalnu velicinu sekcije!" << endl;
    exit(-1);
  }
}

void process_CSRRD_INSTR(string *csr, string *gpr)
{
  if (!currSection || !currSection->code)
  {
    cerr << "Greska: CSRRD instrukcija mora biti unutar sekcije!" << endl;
    return;
  }

  int sNum = 0;
  if (*csr == "%handler")
  {
    sNum = 1;
  }
  else if (*csr == "%cause")
  {
    sNum = 2;
  }
  else if (*csr != "%status")
  {
    cerr << "Greska: Nepoznat CSR registar: " << *csr << endl;
    return;
  }

  int dNum = stoi(gpr->substr(1)); // Parsira "rX" -> X

  currSection->code->push_back(0x90);                 // Op kod za CSRRD
  currSection->code->push_back(((dNum << 4) | sNum)); // AAAA = gpr, BBBB = csr
  currSection->code->push_back(0x00);
  currSection->code->push_back(0x00);

  locationCounter += 4;

  if (locationCounter > 4096)
  {
    cout << "ERROR in CSRRD: Section shouldn't be larger than 4096B";
    exit(-1);
  }
}

void process_CSRWR_INSTR(string *gpr, string *csr)
{
  if (!currSection || !currSection->code)
  {
    cerr << "Greska: CSRWR instrukcija mora biti unutar sekcije!" << endl;
    return;
  }

  int sNum = 0;
  if (*csr == "%handler")
  {
    sNum = 1;
  }
  else if (*csr == "%cause")
  {
    sNum = 2;
  }
  else if (*csr != "%status")
  {
    cerr << "Greska: Nepoznat CSR registar: " << *csr << endl;
    return;
  }

  int dNum = stoi(gpr->substr(1)); // Parsira "rX" -> X

  currSection->code->push_back(0x94);                 // Op kod za CSRRD
  currSection->code->push_back(((dNum << 4) | sNum)); // AAAA = gpr, BBBB = csr
  currSection->code->push_back(0x00);
  currSection->code->push_back(0x00);

  locationCounter += 4;

  if (locationCounter > 4096)
  {
    cout << "ERROR in CSRWR: Section shouldn't be larger than 4096B";
    exit(-1);
  }
}