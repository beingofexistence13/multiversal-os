/*
 * PROJECT:     shell32
 * LICENSE:     LGPL-2.1+ (https://spdx.org/licenses/LGPL-2.1+)
 * PURPOSE:     Utility functions
 * COPYRIGHT:   Copyright 2023 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
 */

#include "precomp.h"

WINE_DEFAULT_DEBUG_CHANNEL(shell);

static BOOL OpenEffectiveToken(DWORD DesiredAccess, HANDLE *phToken)
{
    BOOL ret;

    if (phToken == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    *phToken = NULL;

    ret = OpenThreadToken(GetCurrentThread(), DesiredAccess, FALSE, phToken);
    if (!ret && GetLastError() == ERROR_NO_TOKEN)
        ret = OpenProcessToken(GetCurrentProcess(), DesiredAccess, phToken);

    return ret;
}

/*************************************************************************
 *                SHOpenEffectiveToken (SHELL32.235)
 */
EXTERN_C BOOL WINAPI SHOpenEffectiveToken(_Out_ LPHANDLE phToken)
{
    TRACE("%p\n", phToken);
    return OpenEffectiveToken(TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, phToken);
}

/*************************************************************************
 *                SHGetUserSessionId (SHELL32.248)
 */
EXTERN_C DWORD WINAPI SHGetUserSessionId(_In_opt_ HANDLE hToken)
{
    DWORD dwSessionId, dwLength;
    BOOL bOpenToken = FALSE;

    TRACE("%p\n", hToken);

    if (!hToken)
        bOpenToken = SHOpenEffectiveToken(&hToken);

    if (!hToken ||
        !GetTokenInformation(hToken, TokenSessionId, &dwSessionId, sizeof(dwSessionId), &dwLength))
    {
        dwSessionId = 0;
    }

    if (bOpenToken)
        CloseHandle(hToken);

    return dwSessionId;
}

/*************************************************************************
 *                SHInvokePrivilegedFunctionW (SHELL32.246)
 */
EXTERN_C
HRESULT WINAPI
SHInvokePrivilegedFunctionW(
    _In_z_ LPCWSTR pszName,
    _In_ PRIVILEGED_FUNCTION fn,
    _In_opt_ LPARAM lParam)
{
    TRACE("(%s %p %p)\n", debugstr_w(pszName), fn, lParam);

    if (!pszName || !fn)
        return E_INVALIDARG;

    HANDLE hToken = NULL;
    TOKEN_PRIVILEGES NewPriv, PrevPriv;
    BOOL bAdjusted = FALSE;

    if (SHOpenEffectiveToken(&hToken) &&
        ::LookupPrivilegeValueW(NULL, pszName, &NewPriv.Privileges[0].Luid))
    {
        NewPriv.PrivilegeCount = 1;
        NewPriv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        DWORD dwReturnSize;
        bAdjusted = ::AdjustTokenPrivileges(hToken, FALSE, &NewPriv,
                                            sizeof(PrevPriv), &PrevPriv, &dwReturnSize);
    }

    HRESULT hr = fn(lParam);

    if (bAdjusted)
        ::AdjustTokenPrivileges(hToken, FALSE, &PrevPriv, 0, NULL, NULL);

    if (hToken)
        ::CloseHandle(hToken);

    return hr;
}

/*************************************************************************
 *                SHTestTokenPrivilegeW (SHELL32.236)
 *
 * @see http://undoc.airesoft.co.uk/shell32.dll/SHTestTokenPrivilegeW.php
 */
EXTERN_C
BOOL WINAPI
SHTestTokenPrivilegeW(_In_opt_ HANDLE hToken, _In_z_ LPCWSTR lpName)
{
    LUID Luid;
    DWORD dwLength;
    PTOKEN_PRIVILEGES pTokenPriv;
    HANDLE hNewToken = NULL;
    BOOL ret = FALSE;

    TRACE("(%p, %s)\n", hToken, debugstr_w(lpName));

    if (!lpName)
        return FALSE;

    if (!hToken)
    {
        if (!SHOpenEffectiveToken(&hNewToken))
            goto Quit;

        if (!hNewToken)
            return FALSE;

        hToken = hNewToken;
    }

    if (!LookupPrivilegeValueW(NULL, lpName, &Luid))
        return FALSE;

    dwLength = 0;
    if (!GetTokenInformation(hToken, TokenPrivileges, NULL, 0, &dwLength))
        goto Quit;

    pTokenPriv = (PTOKEN_PRIVILEGES)LocalAlloc(LPTR, dwLength);
    if (!pTokenPriv)
        goto Quit;

    if (GetTokenInformation(hToken, TokenPrivileges, pTokenPriv, dwLength, &dwLength))
    {
        UINT iPriv, cPrivs;
        cPrivs = pTokenPriv->PrivilegeCount;
        for (iPriv = 0; !ret && iPriv < cPrivs; ++iPriv)
        {
            ret = RtlEqualLuid(&Luid, &pTokenPriv->Privileges[iPriv].Luid);
        }
    }

    LocalFree(pTokenPriv);

Quit:
    if (hToken == hNewToken)
        CloseHandle(hNewToken);

    return ret;
}

/*************************************************************************
 *                SHGetShellStyleHInstance (SHELL32.749)
 */
EXTERN_C HINSTANCE
WINAPI
SHGetShellStyleHInstance(VOID)
{
    HINSTANCE hInst = NULL;
    WCHAR szPath[MAX_PATH], szColorName[100];
    HRESULT hr;
    CStringW strShellStyle;

    TRACE("SHGetShellStyleHInstance called\n");

    /* First, attempt to load the shellstyle dll from the current active theme */
    hr = GetCurrentThemeName(szPath, _countof(szPath), szColorName, _countof(szColorName), NULL, 0);
    if (FAILED(hr))
        goto DoDefault;

    /* Strip the theme filename */
    PathRemoveFileSpecW(szPath);

    strShellStyle = szPath;
    strShellStyle += L"\\Shell\\";
    strShellStyle += szColorName;
    strShellStyle += L"\\ShellStyle.dll";

    hInst = LoadLibraryExW(strShellStyle, NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (hInst)
        return hInst;

    /* Otherwise, use the version stored in the System32 directory */
DoDefault:
    if (!ExpandEnvironmentStringsW(L"%SystemRoot%\\System32\\ShellStyle.dll",
                                   szPath, _countof(szPath)))
    {
        ERR("Expand failed\n");
        return NULL;
    }
    return LoadLibraryExW(szPath, NULL, LOAD_LIBRARY_AS_DATAFILE);
}

/*************************************************************************
 *                SHCreatePropertyBag (SHELL32.715)
 */
EXTERN_C HRESULT
WINAPI
SHCreatePropertyBag(_In_ REFIID riid, _Out_ void **ppvObj)
{
    return SHCreatePropertyBagOnMemory(STGM_READWRITE, riid, ppvObj);
}

/*************************************************************************
 *                SheRemoveQuotesA (SHELL32.@)
 */
EXTERN_C LPSTR
WINAPI
SheRemoveQuotesA(LPSTR psz)
{
    PCHAR pch;

    if (*psz == '"')
    {
        for (pch = psz + 1; *pch && *pch != '"'; ++pch)
        {
            *(pch - 1) = *pch;
        }

        if (*pch == '"')
            *(pch - 1) = ANSI_NULL;
    }

    return psz;
}

/*************************************************************************
 *                SheRemoveQuotesW (SHELL32.@)
 *
 * ExtractAssociatedIconExW uses this function.
 */
EXTERN_C LPWSTR
WINAPI
SheRemoveQuotesW(LPWSTR psz)
{
    PWCHAR pch;

    if (*psz == L'"')
    {
        for (pch = psz + 1; *pch && *pch != L'"'; ++pch)
        {
            *(pch - 1) = *pch;
        }

        if (*pch == L'"')
            *(pch - 1) = UNICODE_NULL;
    }

    return psz;
}

/*************************************************************************
 *  SHFindComputer [SHELL32.91]
 *
 * Invokes the shell search in My Computer. Used in SHFindFiles.
 * Two parameters are ignored.
 */
EXTERN_C BOOL
WINAPI
SHFindComputer(LPCITEMIDLIST pidlRoot, LPCITEMIDLIST pidlSavedSearch)
{
    UNREFERENCED_PARAMETER(pidlRoot);
    UNREFERENCED_PARAMETER(pidlSavedSearch);

    TRACE("%p %p\n", pidlRoot, pidlSavedSearch);

    IContextMenu *pCM;
    HRESULT hr = CoCreateInstance(CLSID_ShellSearchExt, NULL, CLSCTX_INPROC_SERVER,
                                  IID_IContextMenu, (void **)&pCM);
    if (FAILED(hr))
    {
        ERR("0x%08X\n", hr);
        return hr;
    }

    CMINVOKECOMMANDINFO InvokeInfo = { sizeof(InvokeInfo) };
    InvokeInfo.lpParameters = "{996E1EB1-B524-11D1-9120-00A0C98BA67D}";
    InvokeInfo.nShow = SW_SHOWNORMAL;
    hr = pCM->InvokeCommand(&InvokeInfo);
    pCM->Release();

    return SUCCEEDED(hr);
}

static HRESULT
Int64ToStr(
    _In_ LONGLONG llValue,
    _Out_writes_z_(cchValue) LPWSTR pszValue,
    _In_ UINT cchValue)
{
    WCHAR szBuff[40];
    UINT ich = 0, ichValue;
#if (WINVER >= _WIN32_WINNT_VISTA)
    BOOL bMinus = (llValue < 0);

    if (bMinus)
        llValue = -llValue;
#endif

    if (cchValue <= 0)
        return E_FAIL;

    do
    {
        szBuff[ich++] = (WCHAR)(L'0' + (llValue % 10));
        llValue /= 10;
    } while (llValue != 0 && ich < _countof(szBuff) - 1);

#if (WINVER >= _WIN32_WINNT_VISTA)
    if (bMinus && ich < _countof(szBuff))
        szBuff[ich++] = '-';
#endif

    for (ichValue = 0; ich > 0 && ichValue < cchValue; ++ichValue)
    {
        --ich;
        pszValue[ichValue] = szBuff[ich];
    }

    if (ichValue >= cchValue)
    {
        pszValue[cchValue - 1] = UNICODE_NULL;
        return E_FAIL;
    }

    pszValue[ichValue] = UNICODE_NULL;
    return S_OK;
}

static VOID
Int64GetNumFormat(
    _Out_ NUMBERFMTW *pDest,
    _In_opt_ const NUMBERFMTW *pSrc,
    _In_ DWORD dwNumberFlags,
    _Out_writes_z_(cchDecimal) LPWSTR pszDecimal,
    _In_ INT cchDecimal,
    _Out_writes_z_(cchThousand) LPWSTR pszThousand,
    _In_ INT cchThousand)
{
    WCHAR szBuff[20];

    if (pSrc)
        *pDest = *pSrc;
    else
        dwNumberFlags = 0;

    if (!(dwNumberFlags & FMT_USE_NUMDIGITS))
    {
        GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_IDIGITS, szBuff, _countof(szBuff));
        pDest->NumDigits = StrToIntW(szBuff);
    }

    if (!(dwNumberFlags & FMT_USE_LEADZERO))
    {
        GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_ILZERO, szBuff, _countof(szBuff));
        pDest->LeadingZero = StrToIntW(szBuff);
    }

    if (!(dwNumberFlags & FMT_USE_GROUPING))
    {
        GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SGROUPING, szBuff, _countof(szBuff));
        pDest->Grouping = StrToIntW(szBuff);
    }

    if (!(dwNumberFlags & FMT_USE_DECIMAL))
    {
        GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SDECIMAL, pszDecimal, cchDecimal);
        pDest->lpDecimalSep = pszDecimal;
    }

    if (!(dwNumberFlags & FMT_USE_THOUSAND))
    {
        GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_STHOUSAND, pszThousand, cchThousand);
        pDest->lpThousandSep = pszThousand;
    }

    if (!(dwNumberFlags & FMT_USE_NEGNUMBER))
    {
        GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_INEGNUMBER, szBuff, _countof(szBuff));
        pDest->NegativeOrder = StrToIntW(szBuff);
    }
}

/*************************************************************************
 *  Int64ToString [SHELL32.209]
 *
 * @see http://undoc.airesoft.co.uk/shell32.dll/Int64ToString.php
 */
EXTERN_C
INT WINAPI
Int64ToString(
    _In_ LONGLONG llValue,
    _Out_writes_z_(cchOut) LPWSTR pszOut,
    _In_ UINT cchOut,
    _In_ BOOL bUseFormat,
    _In_opt_ const NUMBERFMTW *pNumberFormat,
    _In_ DWORD dwNumberFlags)
{
    INT ret;
    NUMBERFMTW NumFormat;
    WCHAR szValue[80], szDecimalSep[6], szThousandSep[6];

    Int64ToStr(llValue, szValue, _countof(szValue));

    if (bUseFormat)
    {
        Int64GetNumFormat(&NumFormat, pNumberFormat, dwNumberFlags,
                          szDecimalSep, _countof(szDecimalSep),
                          szThousandSep, _countof(szThousandSep));
        ret = GetNumberFormatW(LOCALE_USER_DEFAULT, 0, szValue, &NumFormat, pszOut, cchOut);
        if (ret)
            --ret;
        return ret;
    }

    if (FAILED(StringCchCopyW(pszOut, cchOut, szValue)))
        return 0;

    return lstrlenW(pszOut);
}

/*************************************************************************
 *  LargeIntegerToString [SHELL32.210]
 *
 * @see http://undoc.airesoft.co.uk/shell32.dll/LargeIntegerToString.php
 */
EXTERN_C
INT WINAPI
LargeIntegerToString(
    _In_ const LARGE_INTEGER *pLargeInt,
    _Out_writes_z_(cchOut) LPWSTR pszOut,
    _In_ UINT cchOut,
    _In_ BOOL bUseFormat,
    _In_opt_ const NUMBERFMTW *pNumberFormat,
    _In_ DWORD dwNumberFlags)
{
    return Int64ToString(pLargeInt->QuadPart, pszOut, cchOut, bUseFormat,
                         pNumberFormat, dwNumberFlags);
}
