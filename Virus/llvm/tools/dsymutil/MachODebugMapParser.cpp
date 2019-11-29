//===- tools/dsymutil/MachODebugMapParser.cpp - Parse STABS debug maps ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "BinaryHolder.h"
#include "DebugMap.h"
#include "MachOUtils.h"
#include "llvm/ADT/Optional.h"
#include "llvm/Object/MachO.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"
#include <vector>

namespace {
using namespace llvm;
using namespace llvm::dsymutil;
using namespace llvm::object;

class MachODebugMapParser {
public:
  MachODebugMapParser(StringRef BinaryPath, ArrayRef<std::string> Archs,
                      StringRef PathPrefix = "",
                      bool PaperTrailWarnings = false, bool Verbose = false)
      : BinaryPath(BinaryPath), Archs(Archs.begin(), Archs.end()),
        PathPrefix(PathPrefix), PaperTrailWarnings(PaperTrailWarnings),
        BinHolder(Verbose), CurrentDebugMapObject(nullptr) {}

  /// Parses and returns the DebugMaps of the input binary. The binary contains
  /// multiple maps in case it is a universal binary.
  /// \returns an error in case the provided BinaryPath doesn't exist
  /// or isn't of a supported type.
  ErrorOr<std::vector<std::unique_ptr<DebugMap>>> parse();

  /// Walk the symbol table and dump it.
  bool dumpStab();

private:
  std::string BinaryPath;
  SmallVector<StringRef, 1> Archs;
  std::string PathPrefix;
  bool PaperTrailWarnings;

  /// Owns the MemoryBuffer for the main binary.
  BinaryHolder BinHolder;
  /// Map of the binary symbol addresses.
  StringMap<uint64_t> MainBinarySymbolAddresses;
  StringRef MainBinaryStrings;
  /// The constructed DebugMap.
  std::unique_ptr<DebugMap> Result;
  /// List of common symbols that need to be added to the debug map.
  std::vector<std::string> CommonSymbols;

  /// Map of the currently processed object file symbol addresses.
  StringMap<Optional<uint64_t>> CurrentObjectAddresses;
  /// Element of the debug map corresponding to the current object file.
  DebugMapObject *CurrentDebugMapObject;

  /// Holds function info while function scope processing.
  const char *CurrentFunctionName;
  uint64_t CurrentFunctionAddress;

  std::unique_ptr<DebugMap> parseOneBinary(const MachOObjectFile &MainBinary,
                                           StringRef BinaryPath);

  void
  switchToNewDebugMapObject(StringRef Filename,
                            sys::TimePoint<std::chrono::seconds> Timestamp);
  void resetParserState();
  uint64_t getMainBinarySymbolAddress(StringRef Name);
  std::vector<StringRef> getMainBinarySymbolNames(uint64_t Value);
  void loadMainBinarySymbols(const MachOObjectFile &MainBinary);
  void loadCurrentObjectFileSymbols(const object::MachOObjectFile &Obj);
  void handleStabSymbolTableEntry(uint32_t StringIndex, uint8_t Type,
                                  uint8_t SectionIndex, uint16_t Flags,
                                  uint64_t Value);

  template <typename STEType> void handleStabDebugMapEntry(const STEType &STE) {
    handleStabSymbolTableEntry(STE.n_strx, STE.n_type, STE.n_sect, STE.n_desc,
                               STE.n_value);
  }

  void addCommonSymbols();

  /// Dump the symbol table output header.
  void dumpSymTabHeader(raw_ostream &OS, StringRef Arch);

  /// Dump the contents of nlist entries.
  void dumpSymTabEntry(raw_ostream &OS, uint64_t Index, uint32_t StringIndex,
                       uint8_t Type, uint8_t SectionIndex, uint16_t Flags,
                       uint64_t Value);

  template <typename STEType>
  void dumpSymTabEntry(raw_ostream &OS, uint64_t Index, const STEType &STE) {
    dumpSymTabEntry(OS, Index, STE.n_strx, STE.n_type, STE.n_sect, STE.n_desc,
                    STE.n_value);
  }
  void dumpOneBinaryStab(const MachOObjectFile &MainBinary,
                         StringRef BinaryPath);

  void Warning(const Twine &Msg, StringRef File = StringRef()) {
    WithColor::warning() << "("
                         << MachOUtils::getArchName(
                                Result->getTriple().getArchName())
                         << ") " << File << " " << Msg << "\n";

    if (PaperTrailWarnings) {
      if (!File.empty())
        Result->addDebugMapObject(File, sys::TimePoint<std::chrono::seconds>());
      if (Result->end() != Result->begin())
        (*--Result->end())->addWarning(Msg.str());
    }
  }
};

} // anonymous namespace

/// Reset the parser state corresponding to the current object
/// file. This is to be called after an object file is finished
/// processing.
void MachODebugMapParser::resetParserState() {
  CommonSymbols.clear();
  CurrentObjectAddresses.clear();
  CurrentDebugMapObject = nullptr;
}

/// Commons symbols won't show up in the symbol map but might need to be
/// relocated. We can add them to the symbol table ourselves by combining the
/// information in the object file (the symbol name) and the main binary (the
/// address).
void MachODebugMapParser::addCommonSymbols() {
  for (auto &CommonSymbol : CommonSymbols) {
    uint64_t CommonAddr = getMainBinarySymbolAddress(CommonSymbol);
    if (CommonAddr == 0) {
      // The main binary doesn't have an address for the given symbol.
      continue;
    }
    if (!CurrentDebugMapObject->addSymbol(CommonSymbol, None /*ObjectAddress*/,
                                          CommonAddr, 0 /*size*/)) {
      // The symbol is already present.
      continue;
    }
  }
}

/// Create a new DebugMapObject. This function resets the state of the
/// parser that was referring to the last object file and sets
/// everything up to add symbols to the new one.
void MachODebugMapParser::switchToNewDebugMapObject(
    StringRef Filename, sys::TimePoint<std::chrono::seconds> Timestamp) {
  addCommonSymbols();
  resetParserState();

  SmallString<80> Path(PathPrefix);
  sys::path::append(Path, Filename);

  auto ObjectEntry = BinHolder.getObjectEntry(Path, Timestamp);
  if (!ObjectEntry) {
    auto Err = ObjectEntry.takeError();
    Warning("unable to open object file: " + toString(std::move(Err)),
            Path.str());
    return;
  }

  auto Object = ObjectEntry->getObjectAs<MachOObjectFile>(Result->getTriple());
  if (!Object) {
    auto Err = Object.takeError();
    Warning("unable to open object file: " + toString(std::move(Err)),
            Path.str());
    return;
  }

  CurrentDebugMapObject =
      &Result->addDebugMapObject(Path, Timestamp, MachO::N_OSO);
  loadCurrentObjectFileSymbols(*Object);
}

static std::string getArchName(const object::MachOObjectFile &Obj) {
  Triple T = Obj.getArchTriple();
  return T.getArchName();
}

std::unique_ptr<DebugMap>
MachODebugMapParser::parseOneBinary(const MachOObjectFile &MainBinary,
                                    StringRef BinaryPath) {
  loadMainBinarySymbols(MainBinary);
  ArrayRef<uint8_t> UUID = MainBinary.getUuid();
  Result = std::make_unique<DebugMap>(MainBinary.getArchTriple(), BinaryPath, UUID);
  MainBinaryStrings = MainBinary.getStringTableData();
  for (const SymbolRef &Symbol : MainBinary.symbols()) {
    const DataRefImpl &DRI = Symbol.getRawDataRefImpl();
    if (MainBinary.is64Bit())
      handleStabDebugMapEntry(MainBinary.getSymbol64TableEntry(DRI));
    else
      handleStabDebugMapEntry(MainBinary.getSymbolTableEntry(DRI));
  }

  resetParserState();
  return std::move(Result);
}

// Table that maps Darwin's Mach-O stab constants to strings to allow printing.
// llvm-nm has very similar code, the strings used here are however slightly
// different and part of the interface of dsymutil (some project's build-systems
// parse the ouptut of dsymutil -s), thus they shouldn't be changed.
struct DarwinStabName {
  uint8_t NType;
  const char *Name;
};

static const struct DarwinStabName DarwinStabNames[] = {
    {MachO::N_GSYM, "N_GSYM"},    {MachO::N_FNAME, "N_FNAME"},
    {MachO::N_FUN, "N_FUN"},      {MachO::N_STSYM, "N_STSYM"},
    {MachO::N_LCSYM, "N_LCSYM"},  {MachO::N_BNSYM, "N_BNSYM"},
    {MachO::N_PC, "N_PC"},        {MachO::N_AST, "N_AST"},
    {MachO::N_OPT, "N_OPT"},      {MachO::N_RSYM, "N_RSYM"},
    {MachO::N_SLINE, "N_SLINE"},  {MachO::N_ENSYM, "N_ENSYM"},
    {MachO::N_SSYM, "N_SSYM"},    {MachO::N_SO, "N_SO"},
    {MachO::N_OSO, "N_OSO"},      {MachO::N_LSYM, "N_LSYM"},
    {MachO::N_BINCL, "N_BINCL"},  {MachO::N_SOL, "N_SOL"},
    {MachO::N_PARAMS, "N_PARAM"}, {MachO::N_VERSION, "N_VERS"},
    {MachO::N_OLEVEL, "N_OLEV"},  {MachO::N_PSYM, "N_PSYM"},
    {MachO::N_EINCL, "N_EINCL"},  {MachO::N_ENTRY, "N_ENTRY"},
    {MachO::N_LBRAC, "N_LBRAC"},  {MachO::N_EXCL, "N_EXCL"},
    {MachO::N_RBRAC, "N_RBRAC"},  {MachO::N_BCOMM, "N_BCOMM"},
    {MachO::N_ECOMM, "N_ECOMM"},  {MachO::N_ECOML, "N_ECOML"},
    {MachO::N_LENG, "N_LENG"},    {0, nullptr}};

static const char *getDarwinStabString(uint8_t NType) {
  for (unsigned i = 0; DarwinStabNames[i].Name; i++) {
    if (DarwinStabNames[i].NType == NType)
      return DarwinStabNames[i].Name;
  }
  return nullptr;
}

void MachODebugMapParser::dumpSymTabHeader(raw_ostream &OS, StringRef Arch) {
  OS << "-----------------------------------"
        "-----------------------------------\n";
  OS << "Symbol table for: '" << BinaryPath << "' (" << Arch.data() << ")\n";
  OS << "-----------------------------------"
        "-----------------------------------\n";
  OS << "Index    n_strx   n_type             n_sect n_desc n_value\n";
  OS << "======== -------- ------------------ ------ ------ ----------------\n";
}

void MachODebugMapParser::dumpSymTabEntry(raw_ostream &OS, uint64_t Index,
                                          uint32_t StringIndex, uint8_t Type,
                                          uint8_t SectionIndex, uint16_t Flags,
                                          uint64_t Value) {
  // Index
  OS << '[' << format_decimal(Index, 6)
     << "] "
     // n_strx
     << format_hex_no_prefix(StringIndex, 8)
     << ' '
     // n_type...
     << format_hex_no_prefix(Type, 2) << " (";

  if (Type & MachO::N_STAB)
    OS << left_justify(getDarwinStabString(Type), 13);
  else {
    if (Type & MachO::N_PEXT)
      OS << "PEXT ";
    else
      OS << "     ";
    switch (Type & MachO::N_TYPE) {
    case MachO::N_UNDF: // 0x0 undefined, n_sect == NO_SECT
      OS << "UNDF";
      break;
    case MachO::N_ABS: // 0x2 absolute, n_sect == NO_SECT
      OS << "ABS ";
      break;
    case MachO::N_SECT: // 0xe defined in section number n_sect
      OS << "SECT";
      break;
    case MachO::N_PBUD: // 0xc prebound undefined (defined in a dylib)
      OS << "PBUD";
      break;
    case MachO::N_INDR: // 0xa indirect
      OS << "INDR";
      break;
    default:
      OS << format_hex_no_prefix(Type, 2) << "    ";
      break;
    }
    if (Type & MachO::N_EXT)
      OS << " EXT";
    else
      OS << "    ";
  }

  OS << ") "
     // n_sect
     << format_hex_no_prefix(SectionIndex, 2)
     << "     "
     // n_desc
     << format_hex_no_prefix(Flags, 4)
     << "   "
     // n_value
     << format_hex_no_prefix(Value, 16);

  const char *Name = &MainBinaryStrings.data()[StringIndex];
  if (Name && Name[0])
    OS << " '" << Name << "'";

  OS << "\n";
}

void MachODebugMapParser::dumpOneBinaryStab(const MachOObjectFile &MainBinary,
                                            StringRef BinaryPath) {
  loadMainBinarySymbols(MainBinary);
  MainBinaryStrings = MainBinary.getStringTableData();
  raw_ostream &OS(llvm::outs());

  dumpSymTabHeader(OS, getArchName(MainBinary));
  uint64_t Idx = 0;
  for (const SymbolRef &Symbol : MainBinary.symbols()) {
    const DataRefImpl &DRI = Symbol.getRawDataRefImpl();
    if (MainBinary.is64Bit())
      dumpSymTabEntry(OS, Idx, MainBinary.getSymbol64TableEntry(DRI));
    else
      dumpSymTabEntry(OS, Idx, MainBinary.getSymbolTableEntry(DRI));
    Idx++;
  }

  OS << "\n\n";
  resetParserState();
}

static bool shouldLinkArch(SmallVectorImpl<StringRef> &Archs, StringRef Arch) {
  if (Archs.empty() || is_contained(Archs, "all") || is_contained(Archs, "*"))
    return true;

  if (Arch.startswith("arm") && Arch != "arm64" && is_contained(Archs, "arm"))
    return true;

  SmallString<16> ArchName = Arch;
  if (Arch.startswith("thumb"))
    ArchName = ("arm" + Arch.substr(5)).str();

  return is_contained(Archs, ArchName);
}

bool MachODebugMapParser::dumpStab() {
  auto ObjectEntry = BinHolder.getObjectEntry(BinaryPath);
  if (!ObjectEntry) {
    auto Err = ObjectEntry.takeError();
    WithColor::error() << "cannot load '" << BinaryPath
                       << "': " << toString(std::move(Err)) << '\n';
    return false;
  }

  auto Objects = ObjectEntry->getObjectsAs<MachOObjectFile>();
  if (!Objects) {
    auto Err = Objects.takeError();
    WithColor::error() << "cannot get '" << BinaryPath
                       << "' as MachO file: " << toString(std::move(Err))
                       << "\n";
    return false;
  }

  for (const auto *Object : *Objects)
    if (shouldLinkArch(Archs, Object->getArchTriple().getArchName()))
      dumpOneBinaryStab(*Object, BinaryPath);

  return true;
}

/// This main parsing routine tries to open the main binary and if
/// successful iterates over the STAB entries. The real parsing is
/// done in handleStabSymbolTableEntry.
ErrorOr<std::vector<std::unique_ptr<DebugMap>>> MachODebugMapParser::parse() {
  auto ObjectEntry = BinHolder.getObjectEntry(BinaryPath);
  if (!ObjectEntry) {
    return errorToErrorCode(ObjectEntry.takeError());
  }

  auto Objects = ObjectEntry->getObjectsAs<MachOObjectFile>();
  if (!Objects) {
    return errorToErrorCode(ObjectEntry.takeError());
  }

  std::vector<std::unique_ptr<DebugMap>> Results;
  for (const auto *Object : *Objects)
    if (shouldLinkArch(Archs, Object->getArchTriple().getArchName()))
      Results.push_back(parseOneBinary(*Object, BinaryPath));

  return std::move(Results);
}

/// Interpret the STAB entries to fill the DebugMap.
void MachODebugMapParser::handleStabSymbolTableEntry(uint32_t StringIndex,
                                                     uint8_t Type,
                                                     uint8_t SectionIndex,
                                                     uint16_t Flags,
                                                     uint64_t Value) {
  if (!(Type & MachO::N_STAB))
    return;

  const char *Name = &MainBinaryStrings.data()[StringIndex];

  // An N_OSO entry represents the start of a new object file description.
  if (Type == MachO::N_OSO)
    return switchToNewDebugMapObject(Name, sys::toTimePoint(Value));

  if (Type == MachO::N_AST) {
    SmallString<80> Path(PathPrefix);
    sys::path::append(Path, Name);
    Result->addDebugMapObject(Path, sys::toTimePoint(Value), Type);
    return;
  }

  // If the last N_OSO object file wasn't found, CurrentDebugMapObject will be
  // null. Do not update anything until we find the next valid N_OSO entry.
  if (!CurrentDebugMapObject)
    return;

  uint32_t Size = 0;
  switch (Type) {
  case MachO::N_GSYM:
    // This is a global variable. We need to query the main binary
    // symbol table to find its address as it might not be in the
    // debug map (for common symbols).
    Value = getMainBinarySymbolAddress(Name);
    break;
  case MachO::N_FUN:
    // Functions are scopes in STABS. They have an end marker that
    // contains the function size.
    if (Name[0] == '\0') {
      Size = Value;
      Value = CurrentFunctionAddress;
      Name = CurrentFunctionName;
      break;
    } else {
      CurrentFunctionName = Name;
      CurrentFunctionAddress = Value;
      return;
    }
  case MachO::N_STSYM:
    break;
  default:
    return;
  }

  auto ObjectSymIt = CurrentObjectAddresses.find(Name);

  // If the name of a (non-static) symbol is not in the current object, we
  // check all its aliases from the main binary.
  if (ObjectSymIt == CurrentObjectAddresses.end() && Type != MachO::N_STSYM) {
    for (const auto &Alias : getMainBinarySymbolNames(Value)) {
      ObjectSymIt = CurrentObjectAddresses.find(Alias);
      if (ObjectSymIt != CurrentObjectAddresses.end())
        break;
    }
  }

  if (ObjectSymIt == CurrentObjectAddresses.end()) {
    Warning("could not find object file symbol for symbol " + Twine(Name));
    return;
  }

  if (!CurrentDebugMapObject->addSymbol(Name, ObjectSymIt->getValue(), Value,
                                        Size)) {
    Warning(Twine("failed to insert symbol '") + Name + "' in the debug map.");
    return;
  }
}

/// Load the current object file symbols into CurrentObjectAddresses.
void MachODebugMapParser::loadCurrentObjectFileSymbols(
    const object::MachOObjectFile &Obj) {
  CurrentObjectAddresses.clear();

  for (auto Sym : Obj.symbols()) {
    uint64_t Addr = Sym.getValue();
    Expected<StringRef> Name = Sym.getName();
    if (!Name) {
      // TODO: Actually report errors helpfully.
      consumeError(Name.takeError());
      continue;
    }
    // The value of some categories of symbols isn't meaningful. For
    // example common symbols store their size in the value field, not
    // their address. Absolute symbols have a fixed address that can
    // conflict with standard symbols. These symbols (especially the
    // common ones), might still be referenced by relocations. These
    // relocations will use the symbol itself, and won't need an
    // object file address. The object file address field is optional
    // in the DebugMap, leave it unassigned for these symbols.
    uint32_t Flags = Sym.getFlags();
    if (Flags & SymbolRef::SF_Absolute) {
      CurrentObjectAddresses[*Name] = None;
    } else if (Flags & SymbolRef::SF_Common) {
      CurrentObjectAddresses[*Name] = None;
      CommonSymbols.push_back(*Name);
    } else {
      CurrentObjectAddresses[*Name] = Addr;
    }
  }
}

/// Lookup a symbol address in the main binary symbol table. The
/// parser only needs to query common symbols, thus not every symbol's
/// address is available through this function.
uint64_t MachODebugMapParser::getMainBinarySymbolAddress(StringRef Name) {
  auto Sym = MainBinarySymbolAddresses.find(Name);
  if (Sym == MainBinarySymbolAddresses.end())
    return 0;
  return Sym->second;
}

/// Get all symbol names in the main binary for the given value.
std::vector<StringRef>
MachODebugMapParser::getMainBinarySymbolNames(uint64_t Value) {
  std::vector<StringRef> Names;
  for (const auto &Entry : MainBinarySymbolAddresses) {
    if (Entry.second == Value)
      Names.push_back(Entry.first());
  }
  return Names;
}

/// Load the interesting main binary symbols' addresses into
/// MainBinarySymbolAddresses.
void MachODebugMapParser::loadMainBinarySymbols(
    const MachOObjectFile &MainBinary) {
  section_iterator Section = MainBinary.section_end();
  MainBinarySymbolAddresses.clear();
  for (const auto &Sym : MainBinary.symbols()) {
    Expected<SymbolRef::Type> TypeOrErr = Sym.getType();
    if (!TypeOrErr) {
      // TODO: Actually report errors helpfully.
      consumeError(TypeOrErr.takeError());
      continue;
    }
    SymbolRef::Type Type = *TypeOrErr;
    // Skip undefined and STAB entries.
    if ((Type == SymbolRef::ST_Debug) || (Type == SymbolRef::ST_Unknown))
      continue;
    // In theory, the only symbols of interest are the global variables. These
    // are the only ones that need to be queried because the address of common
    // data won't be described in the debug map. All other addresses should be
    // fetched for the debug map. In reality, by playing with 'ld -r' and
    // export lists, you can get symbols described as N_GSYM in the debug map,
    // but associated with a local symbol. Gather all the symbols, but prefer
    // the global ones.
    uint8_t SymType =
        MainBinary.getSymbolTableEntry(Sym.getRawDataRefImpl()).n_type;
    bool Extern = SymType & (MachO::N_EXT | MachO::N_PEXT);
    Expected<section_iterator> SectionOrErr = Sym.getSection();
    if (!SectionOrErr) {
      // TODO: Actually report errors helpfully.
      consumeError(SectionOrErr.takeError());
      continue;
    }
    Section = *SectionOrErr;
    if (Section == MainBinary.section_end() || Section->isText())
      continue;
    uint64_t Addr = Sym.getValue();
    Expected<StringRef> NameOrErr = Sym.getName();
    if (!NameOrErr) {
      // TODO: Actually report errors helpfully.
      consumeError(NameOrErr.takeError());
      continue;
    }
    StringRef Name = *NameOrErr;
    if (Name.size() == 0 || Name[0] == '\0')
      continue;
    // Override only if the new key is global.
    if (Extern)
      MainBinarySymbolAddresses[Name] = Addr;
    else
      MainBinarySymbolAddresses.try_emplace(Name, Addr);
  }
}

namespace llvm {
namespace dsymutil {
llvm::ErrorOr<std::vector<std::unique_ptr<DebugMap>>>
parseDebugMap(StringRef InputFile, ArrayRef<std::string> Archs,
              StringRef PrependPath, bool PaperTrailWarnings, bool Verbose,
              bool InputIsYAML) {
  if (InputIsYAML)
    return DebugMap::parseYAMLDebugMap(InputFile, PrependPath, Verbose);

  MachODebugMapParser Parser(InputFile, Archs, PrependPath, PaperTrailWarnings,
                             Verbose);
  return Parser.parse();
}

bool dumpStab(StringRef InputFile, ArrayRef<std::string> Archs,
              StringRef PrependPath) {
  MachODebugMapParser Parser(InputFile, Archs, PrependPath, false);
  return Parser.dumpStab();
}
} // namespace dsymutil
} // namespace llvm
