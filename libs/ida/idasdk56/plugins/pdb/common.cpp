
static const char spath_prefix[] = "srv*";
static const char spath_suffix[] = "*http://msdl.microsoft.com/download/symbols";

const char *print_pdb_register(int machine, int reg);

//----------------------------------------------------------------------
// Common code for PDB handling
//----------------------------------------------------------------------
class CCallback : public IDiaLoadCallback2, public IDiaReadExeAtRVACallback
{
  int m_nRefCount;
  ea_t m_load_address;
public:
  CCallback(): m_load_address(BADADDR) { m_nRefCount = 0; }

  void SetLoadAddress(ea_t load_address) { m_load_address = load_address; }

  //IUnknown
  ULONG STDMETHODCALLTYPE AddRef()
  {
    m_nRefCount++;
    return m_nRefCount;
  }
  ULONG STDMETHODCALLTYPE Release()
  {
    if ( (--m_nRefCount) == 0 )
    {
      delete this;
      return 0;
    }
    return m_nRefCount;
  }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID rid, void **ppUnk)
  {
    if ( ppUnk == NULL )
      return E_INVALIDARG;
    if ( rid == __uuidof( IDiaLoadCallback2 ) )
        *ppUnk = (IDiaLoadCallback2 *)this;
    else if ( rid == __uuidof( IDiaLoadCallback ) )
        *ppUnk = (IDiaLoadCallback *)this;
#ifndef BUILDING_EFD
    else if ( rid == __uuidof( IDiaReadExeAtRVACallback ) && m_load_address != BADADDR )
        *ppUnk = (IDiaReadExeAtRVACallback *)this;
#endif
    else if ( rid == __uuidof( IUnknown ) )
        *ppUnk = (IUnknown *)(IDiaLoadCallback *)this;
    else
        *ppUnk = NULL;
    if ( *ppUnk != NULL )
    {
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE NotifyDebugDir(
              BOOL /* fExecutable */,
              DWORD /* cbData */,
              BYTE /* data */ []) // really a const struct _IMAGE_DEBUG_DIRECTORY *
  {
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE NotifyOpenDBG(
              LPCOLESTR /* dbgPath */,
              HRESULT /* resultCode */)
  {
    // qprintf(L"opening %s...\n", dbgPath);
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE NotifyOpenPDB(
              LPCOLESTR /* pdbPath */,
              HRESULT /* resultCode */)
  {
    // qprintf(L"opening %s...\n", pdbPath);
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE RestrictRegistryAccess()
  {
    // return hr != S_OK to prevent querying the registry for symbol search paths
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE RestrictSymbolServerAccess()
  {
    // return hr != S_OK to prevent accessing a symbol server
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE RestrictOriginalPathAccess()
  {
    // return hr != S_OK to prevent querying the registry for symbol search paths
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE RestrictReferencePathAccess()
  {
    // return hr != S_OK to prevent accessing a symbol server
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE RestrictDBGAccess()
  {
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE RestrictSystemRootAccess()
  {
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE ReadExecutableAtRVA ( 
   DWORD  relativeVirtualAddress,
   DWORD  cbData,
   DWORD* pcbData,
   BYTE   data[] )
  {
#ifndef BUILDING_EFD
    if ( get_many_bytes(m_load_address + relativeVirtualAddress, data, cbData) )
    {
      *pcbData = cbData;
      return S_OK;
    }
#else
    qnotused(relativeVirtualAddress);
    qnotused(cbData);
    qnotused(pcbData);
    qnotused(data);
#endif
    return S_FALSE;
  }
};

//---------------------------------------------------------------------------
template<class T> void print_generic(T t)
{
  IDiaPropertyStorage *pPropertyStorage;
  HRESULT hr = t->QueryInterface(__uuidof(IDiaPropertyStorage), (void **)&pPropertyStorage);
  if ( hr == S_OK )
  {
    print_property_storage(pPropertyStorage);
    pPropertyStorage->Release();
  }
}

//---------------------------------------------------------------------------
static const char *pdberr(int code)
{
  switch ( code )
  {                         // tab in first pos is flag for replace warning to msg
    case E_PDB_NOT_FOUND:   return "Failed to open the file, or the file has an invalid format.";
    case E_PDB_FORMAT:      return "Attempted to access a file with an obsolete format.";
    case E_PDB_INVALID_SIG: return "Signature does not match.";
    case E_PDB_INVALID_AGE: return "Age does not match.";
    case E_INVALIDARG:      return "Invalid parameter.";
    case E_UNEXPECTED:      return "Data source has already been prepared.";
  }
  return winerr(code);
}

//----------------------------------------------------------------------
size_t get_symbol_length(IDiaSymbol *sym)
{
  DWORD64 size = 0;
  DWORD tag = 0;
  sym->get_symTag(&tag);
  if ( tag == SymTagData )
  {
    IDiaSymbol *pType;
    if ( sym->get_type(&pType) == S_OK )
    {
      pType->get_length(&size);
      pType->Release();
    }
  }
  else
  {
    sym->get_length(&size);
  }
  return size_t(size);
}

//----------------------------------------------------------------------
struct children_visitor_t
{
  virtual HRESULT visit_child(IDiaSymbol *child) = 0;
  virtual ~children_visitor_t() {}
};

static HRESULT for_all_children(IDiaSymbol *sym, enum SymTagEnum type, children_visitor_t &cv)
{
  IDiaEnumSymbols *pEnumSymbols;
  HRESULT hr = sym->findChildren(type, NULL, nsNone, &pEnumSymbols);
  if ( SUCCEEDED(hr) )
  {
    while ( true )
    {
      ULONG celt = 0;
      IDiaSymbol *pChild;
      hr = pEnumSymbols->Next(1, &pChild, &celt);
      if ( FAILED(hr) || celt != 1 )
      {
        hr = S_OK; // end of enumeration
        break;
      }
      hr = cv.visit_child(pChild);
      pChild->Release();
      if ( FAILED(hr) )
        break;
    }
    pEnumSymbols->Release();
  }
  return hr;
}

//----------------------------------------------------------------------
struct file_visitor_t
{
  virtual HRESULT visit_compiland(IDiaSymbol *sym) = 0;
  virtual HRESULT visit_file(IDiaSourceFile *file) = 0;
};

#ifndef COMPILE_PDB_PLUGIN
static HRESULT for_all_files(IDiaSession *pSession, IDiaSymbol *pGlobal, file_visitor_t &fv)
{
  // In order to find the source files, we have to look at the image's compilands/modules
  struct file_helper_t : children_visitor_t
  {
    IDiaSession *pSession;
    file_visitor_t &fv;
    virtual HRESULT visit_child(IDiaSymbol *sym)
    {
      HRESULT hr = fv.visit_compiland(sym);
      if ( SUCCEEDED(hr) )
      {
        IDiaEnumSourceFiles *pEnumSourceFiles;
        if ( SUCCEEDED(pSession->findFile(sym, NULL, nsNone, &pEnumSourceFiles)) )
        {
          DWORD celt;
          IDiaSourceFile *pSourceFile;
          while ( SUCCEEDED(pEnumSourceFiles->Next(1, &pSourceFile, &celt))
               && (celt == 1) )
          {
            hr = fv.visit_file(pSourceFile);
            pSourceFile->Release();
          }
          pEnumSourceFiles->Release();
        }
      }
      return hr;
    }
    file_helper_t(IDiaSession *s, file_visitor_t &v) : pSession(s), fv(v) {}
  };
  file_helper_t fh(pSession, fv);
  return for_all_children(pGlobal, SymTagCompiland, fh);
}
#endif

//----------------------------------------------------------------------
static HRESULT for_all_subtags(IDiaSymbol *pGlobal, enum SymTagEnum type, children_visitor_t &fv)
{
  struct subtag_helper_t : children_visitor_t
  {
    enum SymTagEnum type;
    children_visitor_t &fv;
    virtual HRESULT visit_child(IDiaSymbol *sym)
    {
      return for_all_children(sym, type, fv);
    }
    subtag_helper_t(enum SymTagEnum t, children_visitor_t &v) : type(t), fv(v) {}
  };
  subtag_helper_t fh(type, fv);
  return for_all_children(pGlobal, SymTagCompiland, fh);
}

//----------------------------------------------------------------------
inline HRESULT for_all_funcs(IDiaSymbol *pGlobal, children_visitor_t &fv)
{
  return for_all_subtags(pGlobal, SymTagFunction, fv);
}

//----------------------------------------------------------------------
static const char *print_symtag(uint32 tag)
{
  static const char *const names[] =
  {
    "Null",
    "Exe",
    "Compiland",
    "CompilandDetails",
    "CompilandEnv",
    "Function",
    "Block",
    "Data",
    "Annotation",
    "Label",
    "PublicSymbol",
    "UDT",
    "Enum",
    "FunctionType",
    "PointerType",
    "ArrayType",
    "BaseType",
    "Typedef",
    "BaseClass",
    "Friend",
    "FunctionArgType",
    "FuncDebugStart",
    "FuncDebugEnd",
    "UsingNamespace",
    "VTableShape",
    "VTable",
    "Custom",
    "Thunk",
    "CustomType",
    "ManagedType",
    "Dimension",
  };
  if ( tag < qnumber(names) )
    return names[tag];
  return "???";
}

//----------------------------------------------------------------------
static HRESULT handle_pdb_file(
        const char *input_file,
        const char *user_spath,
        HRESULT handler(IDiaDataSource *pSource,
                        IDiaSession *pSession,
                        IDiaSymbol *pGlobal,
                        int machine_type,
                        int dia_version,
                        void *ud),
        ea_t load_address=BADADDR,
        void *ud=NULL)
{
  class DECLSPEC_UUID("1fbd5ec4-b8e4-4d94-9efe-7ccaf9132c98") DiaSource80;
  class DECLSPEC_UUID("31495af6-0897-4f1e-8dac-1447f10174a1") DiaSource71;
  static const GUID * const d90 = &__uuidof(DiaSource);   // msdia90.dll
  static const GUID * const d80 = &__uuidof(DiaSource80); // msdia80.dll
  static const GUID * const d71 = &__uuidof(DiaSource71); // msdia71.dll

  static const GUID * const msdiav[] = { d90, d80, d71 };
  static const int          diaver[] = { 900, 800, 710 };

  CoInitialize(NULL);
  IDiaDataSource *pSource = NULL;
  HRESULT hr = E_FAIL;
  int dia_ver = 0;

  for ( int i=0; i < qnumber(msdiav); i++ )
  {
    hr = CoCreateInstance(*msdiav[i],
                          NULL,
                          CLSCTX_INPROC_SERVER,
                          __uuidof(IDiaDataSource),
                          (void**)&pSource);
    if ( hr != REGDB_E_CLASSNOTREG )
    {
      dia_ver = diaver[i];
      break;
    }
  }
  if ( SUCCEEDED(hr) )
  {
    CCallback callback;
                        // Receives callbacks from the DIA symbol locating procedure,
                        // thus enabling a user interface to report on the progress of
                        // the location attempt. The client application may optionally
                        // provide a reference to its own implementation of this
                        // virtual base class to the IDiaDataSource::loadDataForExe method.
    callback.AddRef();
    char env_sympath[4096];
    char temp_path[QMAXPATH];
    char spath[sizeof(spath_prefix)+sizeof(temp_path)+sizeof(spath_suffix)];
    qwstring wpath, winput;
    // no symbol path set in CFG file, let us compute default values
    if ( user_spath == NULL )
    {
      // no env var?
      if ( GetEnvironmentVariable("_NT_SYMBOL_PATH", env_sympath, sizeof(env_sympath)) == 0
        || GetLastError() == ERROR_ENVVAR_NOT_FOUND )
      {
        if ( !GetTempPath(sizeof(temp_path), temp_path) )
          temp_path[0] = '\0';
        else
          qstrncat(temp_path, "ida", sizeof(temp_path));
        qsnprintf(spath, sizeof(spath), "%s%s%s", spath_prefix, temp_path, spath_suffix);
        user_spath = spath;
      }
      else
      {
        user_spath = env_sympath;
      }
    }
    c2ustr(user_spath, &wpath);
    c2ustr(input_file, &winput);
    hr = pSource->loadDataFromPdb(winput.c_str());
    IUnknown* punk = (IUnknown *)(IDiaLoadCallback *)(&callback);
    if ( FAILED(hr) )
      hr = pSource->loadDataForExe(winput.c_str(), wpath.c_str(), punk);
    if ( FAILED(hr) )
    {
      // try to use the headers from database
      callback.SetLoadAddress(load_address);
      hr = pSource->loadDataForExe(winput.c_str(), wpath.c_str(), punk);
    }
    if ( SUCCEEDED(hr) )
    {
      // Open a session for querying symbols
      IDiaSession *pSession;
      hr = pSource->openSession(&pSession);
      if ( SUCCEEDED(hr) )
      {
        if ( load_address != BADADDR )
          pSession->put_loadAddress(load_address);
        // Retrieve a reference to the global scope
        IDiaSymbol *pGlobal;
        hr = pSession->get_globalScope(&pGlobal);
        if ( SUCCEEDED(hr) )
        {
          // Set machine type for getting correct register names
          DWORD machine = CV_CFL_80386;
          DWORD dwMachType;
          if ( pGlobal->get_machineType(&dwMachType) == S_OK )
          {
            switch ( dwMachType )
            {
              case IMAGE_FILE_MACHINE_I386:  machine = CV_CFL_80386; break;
              case IMAGE_FILE_MACHINE_IA64:  machine = CV_CFL_IA64;  break;
              case IMAGE_FILE_MACHINE_AMD64: machine = CV_CFL_AMD64; break;
              case IMAGE_FILE_MACHINE_THUMB:
              case IMAGE_FILE_MACHINE_ARM:   machine = CV_CFL_ARM6;  break;
            }
          }
          hr = handler(pSource, pSession, pGlobal, machine, dia_ver, ud);
          pGlobal->Release();
        }
        pSession->Release();
      }
    }
    pSource->Release();
  }
  CoUninitialize();
  return hr;
}

