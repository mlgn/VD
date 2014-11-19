/**
 * @author Micha� Gawin
 */


#ifndef _SAMPLE_PLUGIIN_H_VD_
#define _SAMPLE_PLUGIIN_H_VD_

/**
 * Exported function to create window to change desktop
 * @return handle to plugin's window
 * @param hwnd handle of parent window
 * @param hDll handle to instance of plugin -- not used
 */
extern "C" __declspec(dllexport) HWND __stdcall MakeDialog (HWND hwnd, HANDLE hDll);

/**
 * Function which close plugin's window
 * @return void
 */
extern "C" __declspec(dllexport) VOID __stdcall CloseDialog (HWND hDlg);

#endif //_SAMPLE_PLUGIIN_H_VD_
