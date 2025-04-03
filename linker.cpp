#include "../inc/assembler.hpp"
#include "../inc/linker.hpp"
#include <set>
#include <string.h>
#include <fstream>
#include <algorithm>

using namespace std;

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

      // Da li argument definiše smeštanje sekcije?
      if (currentArg.rfind("-place=", 0) == 0)
      {
        string placeInfo = currentArg.substr(7); // Uklanjamo "-place="

        size_t atSymbol = placeInfo.find('@');
        if (atSymbol != string::npos)
        {
          string secName = placeInfo.substr(0, atSymbol);
          string hexAddr = placeInfo.substr(atSymbol + 1);

          // Ako počinje sa "0x", preskočimo prefiks
          if (hexAddr.rfind("0x", 0) == 0)
          {
            hexAddr = hexAddr.substr(2);
          }
          unsigned long address = 0;
          try {
            address = stoul(hexAddr, nullptr, 16);
          } catch (exception &e) {
            cerr << "Greška: Nevalidna heksadekadna vrednost za -place: " << hexAddr << endl;
            exit(-1);
          }

          if (placeReqs->count(secName))
          {
            cerr << "Greška: Sekcija '" << secName << "' je više puta definisana u -place opciji!" << endl;
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
    }
    inFiles->push_back(move(currFile));
  }
  // sada proveravamo da li ima preklapanja placed sekcija
  vector<pair<unsigned long, string>> ranges;
  for (auto &[secName, section] : *linkerSections)
  {
    ranges.emplace_back(section->base, secName);
  }
  sort(ranges.begin(), ranges.end());

  for (int i = 1; i < ranges.size(); i++)
  {
    auto prev = linkerSections->at(ranges[i - 1].second);
    auto curr = linkerSections->at(ranges[i].second);

    if (prev->base + prev->size > curr->base)
    {
      cerr << "Greška: Sekcije '" << prev->name << "' i '" << curr->name << "' se preklapaju!" << endl;
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
        linkerSymbolTable->insert({asmInfo->sectionName, symb});
        LinkerSection *newSec = new LinkerSection(asmInfo->sectionName, symb->id, nextFreeAddr, asmInfo->size);
        linkerSections->insert({asmInfo->sectionName, newSec});
        nextFreeAddr += asmInfo->size;
      }
      else
      {
        // nadovezivanje na postojeću
        LinkerSection *sec = linkerSections->at(asmInfo->sectionName);
        asmInfo->linkerIdSymbol = sec->idSymbolTable;
        asmInfo->base = sec->base + sec->size;
        sec->size += asmInfo->size;
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

      int oldSymId, oldSecNdx, value;
      int type; // npr. 0 = LOCAL, 1 = GLBL, 2 = EXTRN
      currFile.read(reinterpret_cast<char *>(&oldSymId), sizeof(unsigned int));
      currFile.read(reinterpret_cast<char *>(&oldSecNdx), sizeof(unsigned int));
      currFile.read(reinterpret_cast<char *>(&value), sizeof(unsigned int));
      currFile.read(reinterpret_cast<char *>(&type), sizeof(unsigned int));

      if (type == 1)
      { // GLBL
        if (defined.count(symbName))
        {
          cerr << "Greška: Simbol '" << symbName << "' je višestruko definisan!" << endl;
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
      if (type == 1)
      {
        // Pronađi odgovarajući unos u asmSections da odrediš novu bazu
        for (auto asmInfo : *asmSections)
        {
          if (asmInfo->asmIdFile == fileId && asmInfo->asmIdSymb == oldSecNdx)
          {
            unsigned long newOffset = asmInfo->base + value;
            if (linkerSymbolTable->count(symbName))
            {
              cerr << "Greška: Simbol '" << symbName << "' je već dodat u tabelu simbola!" << endl;
              exit(-1);
            }
            LinkerSymbolTableEntry *newSymb = new LinkerSymbolTableEntry(symbName, 0, 1, asmInfo->linkerIdSymbol, newOffset);
            (*linkerSymbolTable)[symbName] = newSymb;
            break;
          }
        }
      }
    }
  }

  // Na kraju, provera da li postoje nedefinisani simboli
  if (!undefined.empty())
  {
    cerr << "Greška: Postoje nedefinisani simboli: ";
    for (auto symb : undefined)
    {
      cerr << symb << " ";
    }
    cerr << endl;
    exit(-1);
  }
}
