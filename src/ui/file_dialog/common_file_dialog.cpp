#include "common_file_dialog.hpp"

#include <shlwapi.h>
#include <shobjidl.h>

const COMDLG_FILTERSPEC c_rgSaveTypes[] = {{L"Lua files (*.lua)", L"*.lua"},
                                           {L"All Files (*.*)", L"*.*"}};

const size_t INDEX_LUA_SCRIPT = 1;

class CDialogEventHandler : public IFileDialogEvents,
                            public IFileDialogControlEvents {
public:
    CDialogEventHandler() : _cRef(1) {};

    CDialogEventHandler(const CDialogEventHandler&) = default;
    CDialogEventHandler(CDialogEventHandler&&) = delete;
    CDialogEventHandler& operator=(const CDialogEventHandler&) = default;
    CDialogEventHandler& operator=(CDialogEventHandler&&) = delete;

    // IUnknown methods
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) {
        static const QITAB qit[] = {
            QITABENT(CDialogEventHandler, IFileDialogEvents),
            QITABENT(CDialogEventHandler, IFileDialogControlEvents),
            {nullptr},
#pragma warning(suppress : 4838)
        };
        return QISearch(this, qit, riid, ppv);
    }

    IFACEMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&_cRef); }

    IFACEMETHODIMP_(ULONG) Release() {
        long cRef = InterlockedDecrement(&_cRef);
        if (!cRef)
            delete this;
        return cRef;
    }

    // IFileDialogEvents methods
    IFACEMETHODIMP OnFileOk(IFileDialog*) { return S_OK; }
    IFACEMETHODIMP OnFolderChange(IFileDialog*) { return S_OK; }
    IFACEMETHODIMP OnFolderChanging(IFileDialog*, IShellItem*) { return S_OK; }
    IFACEMETHODIMP OnHelp(IFileDialog*) { return S_OK; }
    IFACEMETHODIMP OnSelectionChange(IFileDialog*) { return S_OK; }
    IFACEMETHODIMP OnShareViolation(IFileDialog*, IShellItem*,
                                    FDE_SHAREVIOLATION_RESPONSE*) {
        return S_OK;
    }
    IFACEMETHODIMP OnTypeChange(IFileDialog* pfd);
    IFACEMETHODIMP OnOverwrite(IFileDialog*, IShellItem*,
                               FDE_OVERWRITE_RESPONSE*) {
        return S_OK;
    }

    // IFileDialogControlEvents methods
    IFACEMETHODIMP OnItemSelected(IFileDialogCustomize* pfdc, DWORD dwIDCtl,
                                  DWORD dwIDItem);
    IFACEMETHODIMP OnButtonClicked(IFileDialogCustomize*, DWORD) {
        return S_OK;
    }
    IFACEMETHODIMP OnCheckButtonToggled(IFileDialogCustomize*, DWORD, BOOL) {
        return S_OK;
    }
    IFACEMETHODIMP OnControlActivating(IFileDialogCustomize*, DWORD) {
        return S_OK;
    }

private:
    virtual ~CDialogEventHandler() {}

    long _cRef;
};

HRESULT CDialogEventHandler::OnTypeChange(IFileDialog* pfd) { return S_OK; }

HRESULT CDialogEventHandler::OnItemSelected(IFileDialogCustomize* pfdc,
                                            DWORD dwIDCtl, DWORD dwIDItem) {
    return S_OK;
}

// Instance creation helper
HRESULT CDialogEventHandler_CreateInstance(REFIID riid, void** ppv) {
    *ppv = nullptr;
    auto* pDialogEventHandler = new (std::nothrow) CDialogEventHandler();
    HRESULT hr = pDialogEventHandler ? S_OK : E_OUTOFMEMORY;
    if (SUCCEEDED(hr)) {
        hr = pDialogEventHandler->QueryInterface(riid, ppv);
        pDialogEventHandler->Release();
    }

    return hr;
}

HRESULT open_file_dialog(std::filesystem::path& out_path) {
    // CoCreate the File Open Dialog object.
    IFileDialog* pfd = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
    if (FAILED(hr)) {
        return hr;
    }

    // Create an event handling object, and hook it up to the dialog.
    IFileDialogEvents* pfde = nullptr;
    hr = CDialogEventHandler_CreateInstance(IID_PPV_ARGS(&pfde));
    if (FAILED(hr)) {
        pfd->Release();
        return hr;
    }

    // Hook up the event handler.
    DWORD dwCookie = 0;
    hr = pfd->Advise(pfde, &dwCookie);
    if (FAILED(hr)) {
        pfde->Release();
        pfd->Release();
        return hr;
    }

    // Set the options on the dialog.
    DWORD dwFlags = 0;

    // Before setting, always get the options first in order
    // not to override existing options.
    hr = pfd->GetOptions(&dwFlags);
    if (FAILED(hr)) {
        pfd->Unadvise(dwCookie);
        pfde->Release();
        pfd->Release();
        return hr;
    }

    // In this case, get shell items only for file system items.
    hr = pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM);
    if (FAILED(hr)) {
        pfd->Unadvise(dwCookie);
        pfde->Release();
        pfd->Release();
        return hr;
    }

    // Set the file types to display only.
    // Notice that this is a 1-based array.
    hr = pfd->SetFileTypes(ARRAYSIZE(c_rgSaveTypes), c_rgSaveTypes);
    if (FAILED(hr)) {
        pfd->Unadvise(dwCookie);
        pfde->Release();
        pfd->Release();
        return hr;
    }

    // Set the selected file type index to Word Docs for this example.
    hr = pfd->SetFileTypeIndex(INDEX_LUA_SCRIPT);
    if (FAILED(hr)) {
        pfd->Unadvise(dwCookie);
        pfde->Release();
        pfd->Release();
        return hr;
    }

    // Set the default extension to be ".doc" file.
    hr = pfd->SetDefaultExtension(L"lua");
    if (FAILED(hr)) {
        pfd->Unadvise(dwCookie);
        pfde->Release();
        pfd->Release();
        return hr;
    }

    // Show the dialog
    hr = pfd->Show(nullptr);
    if (FAILED(hr)) {
        pfd->Unadvise(dwCookie);
        pfde->Release();
        pfd->Release();
        return hr;
    }

    // Obtain the result once the user clicks the 'Open' button.
    // The result is an IShellItem object.
    IShellItem* psiResult = nullptr;
    hr = pfd->GetResult(&psiResult);
    if (FAILED(hr)) {
        pfd->Unadvise(dwCookie);
        pfde->Release();
        pfd->Release();
        return hr;
    }

    // We are just going to print out the name of the file for sample sake.
    PWSTR pszFilePath = nullptr;
    hr = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
    if (SUCCEEDED(hr)) {
        CoTaskMemFree(pszFilePath);
        out_path = pszFilePath;
    }
    psiResult->Release();

    return hr;
}