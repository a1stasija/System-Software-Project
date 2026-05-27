#include "../inc/assembler.hpp"
#include "../inc/linker.hpp"
#include <set>
#include <string.h>
#include <fstream>
#include <algorithm>
#include <iomanip>

using namespace std;

int LinkerSymbolTableEntry::cnt = 0;

map<string, LinkerSection *> *linkerSections = new map<string, LinkerSection *>();                      // id iz linkerSymbolTable i pokaz. na LinkerSection
vector<AssemblerSectionInfo *> *asmSections = new vector<AssemblerSectionInfo *>();                     // idFile,sectionName PROVERI MOZE LI OVA STRUKTURA
map<string, LinkerSymbolTableEntry *> *linkerSymbolTable = new map<string, LinkerSymbolTableEntry *>(); // tabela globalnih simbola
map<string, unsigned long> *placeReqs = new map<string, unsigned long>();
set<string> defined;
set<string> undefined;
vector<string> fileNames;
bool hexOpt = false;
bool outputNameDefined = false; // da li je uneta -o opcija

string outputName;

void printLinkerSymbolTable()
{
  cout << "=== TABELA SIMBOLA LINKERA ===\n";
  for (map<string, LinkerSymbolTableEntry *>::iterator it = linkerSymbolTable->begin(); it != linkerSymbolTable->end(); ++it)
  {
    string name = it->first;
    LinkerSymbolTableEntry *entry = it->second;

    cout << "Ime: " << name
         << ", ID: " << entry->id
         << ", Value: " << entry->value
         << ", Type: " << entry->type
         << ", Bind: " << entry->bind
         << ", NDX: " << entry->ndx
         << endl;
  }
  cout << "===============================\n";
}
void printLinkerSectionsCode()
{
  cout << "\n=== CODE SEKCIJA PO SEKCIJI ===\n";
  for (map<string, LinkerSection *>::iterator it = linkerSections->begin(); it != linkerSections->end(); ++it)
  {
    cout << "Sekcija: " << it->first << ", base: " << it->second->base << ", size: " << it->second->size << endl;
    cout << "Code: ";
    vector<char> *code = it->second->code;
    for (size_t i = 0; i < code->size(); i++)
    {
      printf("%02X ", static_cast<unsigned char>((*code)[i]));
      if ((i + 1) % 8 == 0)
        cout << endl;
    }
    cout << "\n----------------------------------\n";
  }
}

void printDefinedAndUndefined()
{
  cout << "Definisani simboli:\n";
  for (const auto &symb : defined)
    cout << symb << endl;

  cout << "\nNedefinisani simboli:\n";
  for (const auto &symb : undefined)
    cout << symb << endl;
}
void printAsmSections()
{
  cout << "=== ASM SECTIONS ===\n";
  for (auto const &sec : *asmSections)
  {
    cout << "Ime: " << sec->sectionName
         << ", FileID: " << sec->asmIdFile
         << ", OldSymbID: " << sec->asmIdSymb
         << ", Base: " << sec->base
         << ", Size: " << sec->size
         << ", Placed: " << (sec->placed ? "Yes" : "No")
         << endl;
  }
  cout << "====================\n";
}

int main(int argc, char *argv[])
{

  int i = 1;
  while (i < argc)
  {
    if (strcmp(argv[i], "-o") == 0)
    {
      outputNameDefined = true;
      outputName = argv[i + 1];
      i += 2;
    }
    else if (strcmp(argv[i], "-hex") == 0)
    {
      hexOpt = true;
      if (!outputNameDefined)
      {
        outputName = "default.hex";
      }
      i++;
    }
    else
    {
      string currentArg = argv[i];

      // Da li argument definiše smestanje sekcije?
      if (currentArg.rfind("-place=", 0) == 0)
      {
        string placeInfo = currentArg.substr(7); // Uklanjamo "-place="

        size_t atSymbol = placeInfo.find('@');
        if (atSymbol != string::npos)
        {
          string secName = placeInfo.substr(0, atSymbol);
          string hexAddr = placeInfo.substr(atSymbol + 1);

          // Ako pocinje sa "0x", preskocimo prefiks
          if (hexAddr.rfind("0x", 0) == 0)
          {
            hexAddr = hexAddr.substr(2);
          }
          unsigned long address = 0;
          try
          {
            address = stoul(hexAddr, nullptr, 16);
          }
          catch (exception &e)
          {
            cerr << "Greska: Nevalidna heksadekadna vrednost za -place: " << hexAddr << endl;
            exit(-1);
          }

          if (placeReqs->count(secName))
          {
            cerr << "Greska: Sekcija '" << secName << "' je vise puta definisana u -place opciji!" << endl;
            exit(-1);
          }

          (*placeReqs)[secName] = address;
        }

        i++;
      }
      else
      {
        fileNames.push_back(argv[i]);
        i++;
      }
    }
  }
  if (fileNames.empty())
  {
    cout << "ERROR: linkeru nije zadat nijedan ulazni objektni fajl" << endl;
    exit(-1);
  }
  if (!hexOpt)
  {
    cout << "ERROR: linker ne moze da radi bez -hex komande" << endl;
    exit(-1);
  }

  vector<ifstream> *inFiles = new vector<ifstream>();

  unsigned long nextFreeAddr = 0;

  for (int fileId = 0; fileId < fileNames.size(); fileId++)
  {
    ifstream currFile(fileNames[fileId], ios::binary);
    if (!currFile.is_open())
    {
      cerr << "Upozorenje: Ne mogu da otvorim fajl " << fileNames[fileId] << endl;
      continue;
    }

    size_t numSections;
    currFile.read(reinterpret_cast<char *>(&numSections), sizeof(size_t)); // Broj sekcija
    for (int s = 0; s < numSections; s++)
    {
      size_t strSize;
      string secName;
      currFile.read(reinterpret_cast<char *>(&strSize), sizeof(size_t)); // Velicina stringa
      secName.resize(strSize);
      currFile.read(&secName[0], strSize);

      unsigned int oldSymId;
      unsigned int secSize;
      currFile.read(reinterpret_cast<char *>(&oldSymId), sizeof(unsigned int));
      currFile.read(reinterpret_cast<char *>(&secSize), sizeof(unsigned int));

      bool placed = (placeReqs->count(secName) > 0);

      AssemblerSectionInfo *helperAsm = new AssemblerSectionInfo(secName, oldSymId, fileId, -1, secSize, placed);
      asmSections->push_back(helperAsm);

      vector<char> buffer(secSize);
      currFile.read(buffer.data(), secSize);

      LinkerSection *helperLd = nullptr;

      if (placed)
      {
        auto existing = linkerSymbolTable->find(secName);
        if (existing == linkerSymbolTable->end())
        { // ne postoji u TS linkera, dodaj tu gde place kaze
          helperAsm->base = (*placeReqs)[secName];
          LinkerSymbolTableEntry *symb = new LinkerSymbolTableEntry(secName, 0, 0, 0, (*placeReqs)[secName]);
          //cout << "Pravimo sekciju " <<secName<< " sa ID: "<< symb->id<<endl;
          helperAsm->linkerIdSymbol = symb->id;
          linkerSymbolTable->insert({secName, symb});
          helperLd = new LinkerSection(secName, symb->id, (*placeReqs)[secName], secSize);
          linkerSections->insert({secName, helperLd});
          // preklapanje proveri kada zavrsis sa mapiranjem
        }
        else
        { // postoji u TS linkera -> dodaj na kraj
          auto sec = linkerSections->find(secName);
          helperLd = sec->second;
          helperAsm->base = helperLd->base + helperLd->size;
          helperAsm->linkerIdSymbol = helperLd->idSymbolTable;
          helperLd->size += helperAsm->size;
        }
        if (helperLd && helperLd->code)
        {
          helperLd->code->insert(helperLd->code->end(), buffer.begin(), buffer.end());
        }

        if (nextFreeAddr < helperAsm->base + secSize)
        {
          nextFreeAddr = helperAsm->base + secSize;
        }
      }
      else
      {
        helperAsm->code->insert(helperAsm->code->end(), buffer.begin(), buffer.end());
      }
    }
    inFiles->push_back(move(currFile));
  }
  // sada proveravamo da li ima preklapanja placed sekcija
  vector<pair<unsigned long, string>> ranges;
  for (auto it = linkerSections->begin(); it != linkerSections->end(); ++it)
  {
    string secName = it->first;
    LinkerSection *section = it->second;
    ranges.emplace_back(section->base, secName);
  }
  sort(ranges.begin(), ranges.end());

  for (int i = 1; i < ranges.size(); i++)
  {
    auto prev = linkerSections->at(ranges[i - 1].second);
    auto curr = linkerSections->at(ranges[i].second);

    if (prev->base + prev->size > curr->base)
    {
      cerr << "Greska: Sekcije '" << prev->name << "' i '" << curr->name << "' se preklapaju!" << endl;
      exit(-1);
    }
  }
  // sada mapiramo sekcije koje nisu placed
  for (auto asmInfo : *asmSections)
  {
    if (!asmInfo->placed)
    {
      if (!linkerSections->count(asmInfo->sectionName))
      {
        // nova sekcija
        asmInfo->base = nextFreeAddr;
        LinkerSymbolTableEntry *symb = new LinkerSymbolTableEntry(asmInfo->sectionName, 0, 0, 0, nextFreeAddr);
        asmInfo->linkerIdSymbol = symb->id;
        //cout << "Pravimo sekciju "<<asmInfo->sectionName<<" sa ID: "<<asmInfo->linkerIdSymbol<<endl;
        linkerSymbolTable->insert({asmInfo->sectionName, symb});
        LinkerSection *newSec = new LinkerSection(asmInfo->sectionName, symb->id, nextFreeAddr, asmInfo->size);
        newSec->code->insert(newSec->code->end(), asmInfo->code->begin(), asmInfo->code->end());
        linkerSections->insert({asmInfo->sectionName, newSec});
        nextFreeAddr += asmInfo->size;
      }
      else
      {
        // nadovezivanje na postojeću
        LinkerSection *sec = linkerSections->at(asmInfo->sectionName);
        sec->code->insert(sec->code->end(), asmInfo->code->begin(), asmInfo->code->end());
        asmInfo->linkerIdSymbol = sec->idSymbolTable;
        asmInfo->base = sec->base + sec->size;
        sec->size += asmInfo->size;

        unsigned long sizeToMove = asmInfo->size;
        // moramo i da azuriramo sve sekcije koje su ispod
        //  Pomeranje svih sekcija koje se nalaze iza ove u memoriji
        for (auto it = linkerSections->begin(); it != linkerSections->end(); ++it)
        {
          string otherName = it->first;
          LinkerSection *otherSec = it->second;
          if (otherSec->base > sec->base && !placeReqs->count(otherName))
          {
            otherSec->base += sizeToMove;
          }
        }

        // Azuriranje asmSections koje su u tim sekcijama
        for (auto &otherAsm : *asmSections)
        {
          if (!otherAsm->placed && otherAsm->sectionName != asmInfo->sectionName && otherAsm->base > sec->base)
          {
            otherAsm->base += sizeToMove;
          }
        }

        nextFreeAddr = max(nextFreeAddr, asmInfo->base + asmInfo->size);
      }
    }
  }
  // formiramo tabelu simbola linkera
  for (int fileId = 0; fileId < fileNames.size(); fileId++)
  {
    ifstream &currFile = (*inFiles)[fileId];
    if (!currFile.is_open())
    {
      cerr << "Upozorenje: Ne mogu da otvorim fajl " << fileNames[fileId] << endl;
      continue;
    }

    size_t symCount;
    currFile.read(reinterpret_cast<char *>(&symCount), sizeof(size_t));

    for (size_t s = 0; s < symCount; s++)
    {
      size_t nameLen;
      string symbName;
      currFile.read(reinterpret_cast<char *>(&nameLen), sizeof(size_t));
      symbName.resize(nameLen);
      currFile.read(&symbName[0], nameLen);

      unsigned int oldSymId, oldSecNdx, value;
      unsigned int type; // npr. 0 = LOCAL, 1 = GLBL, 2 = EXTRN
      currFile.read(reinterpret_cast<char *>(&oldSymId), sizeof(unsigned int));
      currFile.read(reinterpret_cast<char *>(&oldSecNdx), sizeof(unsigned int));
      currFile.read(reinterpret_cast<char *>(&value), sizeof(unsigned int));
      currFile.read(reinterpret_cast<char *>(&type), sizeof(unsigned int));


      if (type == 0)
      { // GLBL
        if (defined.count(symbName) && (oldSymId) != (oldSecNdx))
        {
          cerr << "Greska: Simbol '" << symbName << "' je visestruko definisan!" << endl;
          exit(-1);
        }
        defined.insert(symbName);
        undefined.erase(symbName);
      }
      else if (type == 2)
      { // EXTRN
        if (!defined.count(symbName))
        {
          undefined.insert(symbName);
        }
      }
      // Ubacujemo simbol ako je globalni
      if (type == 0 && oldSecNdx != oldSymId)
      {
        // Pronadji odgovarajuci unos u asmSections da odrediš novu bazu
        for (auto asmInfo : *asmSections)
        {
          if (asmInfo->asmIdFile == fileId && asmInfo->asmIdSymb == oldSecNdx)
          {
            unsigned long newOffset = asmInfo->base + value;
            if (linkerSymbolTable->count(symbName) && (oldSymId) != (oldSecNdx))
            {
              cerr << "Greška: Simbol '" << symbName << "' je već dodat u tabelu simbola!" << endl;
              exit(-1);
            }
            LinkerSymbolTableEntry *newSymb = new LinkerSymbolTableEntry(symbName, 0, 1, asmInfo->linkerIdSymbol, newOffset);
            //cout<< "Pravimo globalni simbol "<< symbName << " sa ID: "<<newSymb->id<<endl;
            (*linkerSymbolTable)[symbName] = newSymb;
            break;
          }
        }
      }
    }
  }
  //printLinkerSymbolTable();
  //printDefinedAndUndefined();
  //printAsmSections();
  //printLinkerSectionsCode();

  // provera da li postoje nedefinisani simboli
  if (!undefined.empty())
  {
    cerr << "Greska: Postoje nedefinisani simboli: ";
    for (auto symb : undefined)
    {
      cerr << symb << " ";
    }
    cerr << endl;
    exit(-1);
  }

  // razresavanje relokacija
  for (int fileId = 0; fileId < fileNames.size(); fileId++)
  {
    ifstream &inFile = (*inFiles)[fileId];

    // Broj sekcija
    size_t numRelocSections;
    inFile.read(reinterpret_cast<char *>(&numRelocSections), sizeof(size_t));

    for (size_t i = 0; i < numRelocSections; ++i)
    {
      size_t numRelocs;
      inFile.read(reinterpret_cast<char *>(&numRelocs), sizeof(size_t));

      for (size_t j = 0; j < numRelocs; ++j)
      {
        // Ime simbola
        size_t nameLen;
        inFile.read(reinterpret_cast<char *>(&nameLen), sizeof(size_t));
        string symbName(nameLen, ' ');
        inFile.read(&symbName[0], nameLen);

        unsigned int idSymbol, sectionId, offset, addend;
        unsigned int type;

        inFile.read(reinterpret_cast<char *>(&idSymbol), sizeof(unsigned int));
        inFile.read(reinterpret_cast<char *>(&sectionId), sizeof(unsigned int));
        inFile.read(reinterpret_cast<char *>(&offset), sizeof(unsigned int));
        inFile.read(reinterpret_cast<char *>(&addend), sizeof(unsigned int));
        inFile.read(reinterpret_cast<char *>(&type), sizeof(unsigned int));

        // Nadji baznu adresu sekcije u kojoj se vrsi relokacija
        AssemblerSectionInfo *targetSection = nullptr;
        for (auto asmInfo : *asmSections)
        {
          if (asmInfo->asmIdFile == fileId && asmInfo->asmIdSymb == sectionId)
          {
            targetSection = asmInfo;
            break;
          }
        }

        if (!targetSection)
        {
          cerr << "Greska: Ne mogu da pronadjem sekciju za relokaciju!" << endl;
          exit(-1);
        }

        // Nadji vrednost simbola
        if (!linkerSymbolTable->count(symbName))
        {
          cerr << "Greska: Simbol '" << symbName << "' nije pronadjen tokom razresavanja relokacija!" << endl;
          exit(-1);
        }

        LinkerSymbolTableEntry *s = (*linkerSymbolTable)[symbName];
        unsigned long symbVal = 0;
        if (s->type == 0)
        {
          // Nadji bazu te sekcije (gde je simbol definisan)
          AssemblerSectionInfo *defSection = nullptr;
          for (auto asmInfo : *asmSections)
          {
            if (asmInfo->asmIdFile == fileId && asmInfo->linkerIdSymbol == s->ndx)
            {
              defSection = asmInfo;
              break;
            }
          }
          if (!defSection)
          {
            cerr << "Greska: Ne mogu da pronadjem sekciju u kojoj je simbol '" << symbName << "' definisan!" << endl;
            exit(-1);
          }
          symbVal = defSection->base;
        }
        else
        {
          symbVal = s->value;
        }

        unsigned long writeAddr = targetSection->base + offset;

        LinkerSection *linkerSec = (*linkerSections)[targetSection->sectionName];

        unsigned long result = 0;
        if (type == 0)
        { // PC-relative
          result = symbVal + addend - writeAddr;
        }
        else if (type == 1)
        { // absolute
          result = symbVal + addend;
        }

        // Upis rezultata u code
        for (int b = 0; b < 4; b++)
        {
          (*linkerSec->code)[writeAddr - linkerSec->base + b] = (result >> (8 * b)) & 0xFF;
        }
      }
    }
  }

  // zatvaranje i ispis
  for (int i = 0; i < inFiles->size(); i++)
  {
    (*inFiles)[i].close();
  }

  ofstream outFile(outputName);
  if (!outFile.is_open())
  {
    cerr << "Greska: Ne mogu da otvorim izlazni fajl: " << outputName << endl;
    exit(-1);
  }

  vector<pair<unsigned long, string>> sortedSections;
  for (auto it = linkerSections->begin(); it != linkerSections->end(); ++it)
  {
    string name = it->first;
    LinkerSection *sec = it->second;
    sortedSections.emplace_back(sec->base, name);
  }

  sort(sortedSections.begin(), sortedSections.end());

  for (size_t i = 0; i < sortedSections.size(); i++)
  {
    string secName = sortedSections[i].second;
    LinkerSection *sec = linkerSections->at(secName);

    unsigned long baseAddr = sec->base;
    for (size_t j = 0; j < sec->code->size(); j += 4)
    {
      outFile << hex << setw(8) << setfill('0') << baseAddr + j << ": ";

      for (int k = 0; k < 4 && j + k < sec->code->size(); k++)
      {
        outFile << hex << setw(2) << setfill('0')
                << (static_cast<unsigned int>((unsigned char)(*sec->code)[j + k])) << " ";
      }

      outFile << "\n";
    }
  }

  outFile << endl;

  outFile.close();
}
