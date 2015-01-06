/**
* VirtualDesktop.cpp : Defines the entry point for the application.
* @author Micha� Gawin
*/


#include "VirtualDesktop.h"
#include "Sys.h"
#include "Reg.h"
#include "Tray.h"
#include "WndMgr.h"

// Plugins
#include "WindowsManager.h"
#include "Plugin.h"
#include "About.h"

#include "DesktopMgr\DesktopMgr.h"			// header to shared dll

WindowsOnDesktop windowsOnDesktop[DESKTOPS];			// Struct keeps data for each desktop

extern CPlugin g_PluginUI;


/**
* Function show popup menu
* @return TRUE if successfully created menu, otherwise return FALSE
* @param hwnd handle of window
* @param check point on status of checkbox "Always on top"
*/
BOOL CreatePopupMenuInTray(HWND hwnd, BOOL check)
{
	HINSTANCE hInstance = (HINSTANCE)GetWindowLong(hwnd, GWL_HINSTANCE);
	static HMENU menu = NULL;
	POINT mouse;

	if (menu)
	{
		DestroyMenu(menu);
	}

	menu = CreatePopupMenu();
	if (!menu)
	{
		return FALSE;
	}

	TCHAR szOnTop[MAX_PATH];
	LoadString(hInstance, IDS_ON_TOP, (TCHAR*)szOnTop, sizeof(szOnTop) / sizeof(TCHAR));
	AppendMenu(menu, MF_BYPOSITION | MF_STRING | (check ? MF_CHECKED : MF_UNCHECKED), CMD_AOT, (LPCTSTR)szOnTop);

	TCHAR szContDeskManager[MAX_PATH];
	LoadString(hInstance, IDS_DESK_CONTENT_MANAGER, (TCHAR*)szContDeskManager, sizeof(szContDeskManager) / sizeof(TCHAR));
	AppendMenu(menu, MF_BYPOSITION | MF_STRING, CMD_DSKMGR, (LPCTSTR)szContDeskManager);

	TCHAR szPlugin[MAX_PATH];
	LoadString(hInstance, IDS_PLUGIN, (TCHAR*)szPlugin, sizeof(szPlugin) / sizeof(TCHAR));
	AppendMenu(menu, MF_BYPOSITION | MF_STRING, CMD_PLUGIN, (LPCTSTR)szPlugin);

	AppendMenu(menu, MF_BYPOSITION | MF_SEPARATOR, 0, 0);

	TCHAR szAbout[MAX_PATH];
	LoadString(hInstance, IDS_ABOUT, (TCHAR*)szAbout, sizeof(szAbout) / sizeof(TCHAR));
	AppendMenu(menu, MF_BYPOSITION | MF_STRING, CMD_ABOUT, (LPCTSTR)szAbout);

	AppendMenu(menu, MF_BYPOSITION | MF_SEPARATOR, 0, 0);

	TCHAR szQuit[MAX_PATH];
	LoadString(hInstance, IDS_QUIT, (TCHAR*)szQuit, sizeof(szQuit) / sizeof(TCHAR));
	AppendMenu(menu, MF_BYPOSITION | MF_STRING, CMD_QUIT, (LPCTSTR)szQuit);

	SetMenuDefaultItem(menu, CMD_DSKMGR, FALSE);

	GetCursorPos(&mouse);
	SetForegroundWindow(hwnd);
	TrackPopupMenu(menu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN, mouse.x, mouse.y, 0, hwnd, 0);
	PostMessage(hwnd, WM_NULL, 0, 0);

	return TRUE;
}


// Main Window Procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


int APIENTRY WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR     lpCmdLine,
	int       nCmdShow)
{
	MSG msg;
	HWND hwnd;
	WNDCLASS wndclass;
	memset(&wndclass, 0, sizeof (WNDCLASS));

	if (FindApplication(szClassName))
	{
		TCHAR szAppName[MAX_PATH];
		LoadString(hInstance, IDS_APP_NAME, (TCHAR*)szAppName, sizeof(szAppName) / sizeof(TCHAR));

		TCHAR szAppLaunched[MAX_PATH];
		LoadString(hInstance, IDS_APP_LAUNCHED, (TCHAR*)szAppLaunched, sizeof(szAppLaunched) / sizeof(TCHAR));

		MessageBox(NULL, szAppLaunched, szAppName, MB_OK);
		return -1;
	}

	INITCOMMONCONTROLSEX initControls;
	initControls.dwSize = sizeof (INITCOMMONCONTROLSEX);
	initControls.dwICC = ICC_BAR_CLASSES;
	InitCommonControlsEx(&initControls);	//activate tooltips

	wndclass.style = CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc = WndProc;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = 0;
	wndclass.hInstance = hInstance;
	wndclass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON));
	wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndclass.hbrBackground = (HBRUSH)GetStockObject(DKGRAY_BRUSH);
	wndclass.lpszMenuName = NULL;
	wndclass.lpszClassName = szClassName;

	if (!RegisterClass(&wndclass))
	{
		TCHAR szError[MAX_PATH];
		LoadString(hInstance, IDS_ERROR, (TCHAR*)szError, sizeof(szError) / sizeof(TCHAR));

		TCHAR szNotSuppOS[MAX_PATH];
		LoadString(hInstance, IDS_ERR_NOT_SUPPORTED_OS, (TCHAR*)szNotSuppOS, sizeof(szNotSuppOS) / sizeof(TCHAR));

		MessageBox(NULL, szError, szNotSuppOS, MB_OK | MB_ICONERROR);
		return -1;
	}

	TCHAR szAppName[MAX_PATH];
	LoadString(hInstance, IDS_APP_NAME, (TCHAR*)szAppName, sizeof(szAppName) / sizeof(TCHAR));

	hwnd = CreateWindow(szClassName,
		szAppName,
		WS_POPUP,
		0, 0, 0, 0,
		NULL,
		NULL,
		hInstance,
		NULL
		);

	ShowWindow(hwnd, SW_HIDE);
	UpdateWindow(hwnd);

	// enable hot keys to change desktops
	RegisterHotKey(hwnd, NEXT_WIN, MOD_CONTROL, VK_TAB);
	RegisterHotKey(hwnd, PREV_WIN, MOD_CONTROL | MOD_SHIFT, VK_TAB);

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	UnregisterHotKey(hwnd, NEXT_WIN);
	UnregisterHotKey(hwnd, PREV_WIN);

	return msg.wParam;
}


/* Main Window Procedures */
LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static CRITICAL_SECTION s_criticalSection;
	static HINSTANCE hInstance;							// instance to program
	static HMODULE hSharedLib = NULL;				// handle to instance of shared dll
	static TCHAR szOrginalWallpaper[MAX_PATH];		// hold orginal desktop wallpaper
	static BOOL bAlwaysOnTop = TRUE;						// hold status of check field "Always on top"
	static CTray* tray = NULL;

	TCHAR szWallpaper[MAX_PATH];							// string which is use to create bitmap
	TCHAR szWallpaperTemplate[] = TEXT("Wallpaper#%d");		// wallpaper name template
	TCHAR szDefaultPluginName[] = SZ_PLUGIN_NAME;	// default plugin name

	switch (uMsg)
	{
	case WM_CREATE:
	{
					  InitializeCriticalSection(&s_criticalSection);

					  hInstance = ((LPCREATESTRUCT)lParam)->hInstance;

					  memset(szOrginalWallpaper, 0, sizeof (szOrginalWallpaper));
					  SystemParametersInfo(SPI_GETDESKWALLPAPER, MAX_PATH, szOrginalWallpaper, 0);

					  for (int i = 0; i < DESKTOPS; i++)
					  {
						  windowsOnDesktop[i].szWallpaper = new TCHAR[MAX_PATH];
						  memset(windowsOnDesktop[i].szWallpaper, 0, MAX_PATH * sizeof (TCHAR));
					  }

					  if (!IsRegistryEntry(TEXT("Software"), VD_MAIN_KEY, VD_PLUGIN_KEY))	// create entry in registry
					  {
						  for (int i = 0; i < DESKTOPS; i++)
						  {
							  memset(szWallpaper, 0, sizeof (szWallpaper));
							  _stprintf(szWallpaper, szWallpaperTemplate, i);

							  SystemParametersInfo(SPI_GETDESKWALLPAPER, MAX_PATH, windowsOnDesktop[i].szWallpaper, 0);
							  SetInRegistry(TEXT("Software"), VD_MAIN_KEY, NULL, szWallpaper, (VOID*)windowsOnDesktop[i].szWallpaper, _tcslen(windowsOnDesktop[i].szWallpaper));
						  }

						  SetInRegistry(TEXT("Software"), VD_MAIN_KEY, NULL, VD_PLUGIN_PATH_KEY, (VOID*)szDefaultPluginName, _tcslen(szDefaultPluginName));
						  g_PluginUI.SetFullPath(szDefaultPluginName);	// Set plugin path
					  }
					  else	// configuration already exists in registry 
					  {
						  for (int i = 0; i < DESKTOPS; i++)
						  {
							  memset(szWallpaper, 0, sizeof (szWallpaper));
							  _stprintf(szWallpaper, szWallpaperTemplate, i);

							  if (!GetFromRegistry(TEXT("Software"), VD_MAIN_KEY, NULL, szWallpaper, windowsOnDesktop[i].szWallpaper, MAX_PATH))
							  {
								  SystemParametersInfo(SPI_GETDESKWALLPAPER, MAX_PATH, windowsOnDesktop[i].szWallpaper, 0);
							  }
						  }

						  TCHAR szLibFullPath[MAX_PATH];
						  memset(szLibFullPath, 0, sizeof (TCHAR) * MAX_PATH);

						  if (GetFromRegistry(TEXT("Software"), VD_MAIN_KEY, NULL, VD_PLUGIN_PATH_KEY, szLibFullPath, MAX_PATH))
						  {
							  g_PluginUI.SetFullPath(szLibFullPath);	// Set plugin path
						  }
						  if (_tcslen(windowsOnDesktop[0].szWallpaper) > 0)
						  {
							  SystemParametersInfo(SPI_SETDESKWALLPAPER, _tcslen(windowsOnDesktop[0].szWallpaper), windowsOnDesktop[0].szWallpaper, 0);
						  }
					  }

					  hSharedLib = LoadLibrary(SZ_DESKTOP_MGR);	// load shared library
					  if (!hSharedLib)
					  {
						  TCHAR szAppName[MAX_PATH];
						  LoadString(hInstance, IDS_APP_NAME, (TCHAR*)szAppName, sizeof(szAppName) / sizeof(TCHAR));

						  TCHAR szErrDeskManager[MAX_PATH];
						  LoadString(hInstance, IDS_ERR_NO_DESKTOP_MANAGER, (TCHAR*)szErrDeskManager, sizeof(szErrDeskManager) / sizeof(TCHAR));

						  MessageBox(NULL, szErrDeskManager, szAppName, MB_OK);
					  }

					  if (!g_PluginUI.LoadAll(g_PluginUI.GetFullPath()))	// load GUI plugin
					  {
						  TCHAR szAppName[MAX_PATH];
						  LoadString(hInstance, IDS_APP_NAME, (TCHAR*)szAppName, sizeof(szAppName) / sizeof(TCHAR));

						  TCHAR szNoPlugin[MAX_PATH];
						  LoadString(hInstance, IDS_ERR_NO_PLUGIN, (TCHAR*)szNoPlugin, sizeof(szNoPlugin) / sizeof(TCHAR));

						  MessageBox(NULL, szNoPlugin, szAppName, MB_OK);
					  }

					  tray = new CTray(hwnd, hInstance, LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON)));
					  tray->Show();
					  return 0;
	}
	case WM_PARENTNOTIFY:	// notification while children DestroyWindow
	{
								return 0;
	}
	case WM_TRAY_ICON:
	{
						 switch (lParam)
						 {
						 case WM_LBUTTONDOWN:
						 {
												HWND hDlg = g_PluginUI.m_pfMakeDialog(hwnd, hSharedLib);
												break;
						 }
						 case WM_RBUTTONDOWN:
						 {
												CreatePopupMenuInTray(hwnd, bAlwaysOnTop);
												break;
						 }
						 }
						 return 0;
	}
	case WM_HOTKEY:
	{
					  switch (HIWORD(lParam))
					  {
					  case VK_TAB:
					  {
									 switch (LOWORD(lParam))
									 {
									 case MOD_CONTROL | MOD_SHIFT:
									 {
																	 int desk = GetCurrentDesktop();
																	 desk--;


																	 if (desk < 0 || desk >= DESKTOPS)
																	 {
																		 desk = (desk + DESKTOPS) % DESKTOPS;
																	 }

																	 ChangeDesktop(desk);
																	 break;
									 }
									 case MOD_CONTROL:
									 {
														 int desk = GetCurrentDesktop();
														 desk++;

														 desk = desk % DESKTOPS;

														 ChangeDesktop(desk);
														 break;
									 }
									 default:
									 {
												break;
									 }
									 }
					  }
					  default:
					  {
								 break;
					  }
					  }
					  return 0;
	}
	case WM_CHANGE_DESKTOP:	// lParam - no. of desktop
	{
								EnterCriticalSection(&s_criticalSection);
								INT ret = 0;

								if ((lParam >= 0) && (lParam < DESKTOPS))
								{
									SystemParametersInfo(SPI_SETDESKWALLPAPER, _tcslen(windowsOnDesktop[lParam].szWallpaper), windowsOnDesktop[lParam].szWallpaper, 0);
									HideWindows(hwnd, windowsOnDesktop[GetCurrentDesktop()].table, TRUE);
									// WARNING!!! Work only if icons in resources are in ascending sequence!
									tray->ChangeIcon(LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON + lParam)));

									SetCurrentDesktop(lParam);

									ShowWindows(windowsOnDesktop[GetCurrentDesktop()].table);
									if (bAlwaysOnTop) g_PluginUI.m_pfMakeDialog(hwnd, hSharedLib);
									else g_PluginUI.m_pfCloseDialog();
								}
								else ret = ERR_DESKTOP_NUM;

								LeaveCriticalSection(&s_criticalSection);

								return ret;
	}
	case WM_COMMAND:
	{
					   switch (LOWORD(wParam))
					   {
					   case CMD_AOT:
					   {
									   bAlwaysOnTop = !bAlwaysOnTop;
									   break;
					   }
					   case CMD_DSKMGR:
					   {
										  GetWindowsFromDesktop(hwnd, windowsOnDesktop[GetCurrentDesktop()].table);
										  WindowsOnDesktop wod = {};
										  for (vHandleItor itor = windowsOnDesktop[GetCurrentDesktop()].table.begin(); itor != windowsOnDesktop[GetCurrentDesktop()].table.end();)
										  {
											  if (IsWindow(*itor))
											  {
												  wod.table.push_back(*itor);
												  itor++;
											  }
											  else
											  {
												  itor = windowsOnDesktop[GetCurrentDesktop()].table.erase(itor++);
											  }
										  }
										  if (DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_DESKTOP_MANAGER), hwnd, DlgDesktopManagerProc, (LPARAM)windowsOnDesktop))
										  {
											  HideWindows(hwnd, wod.table, FALSE);
											  ShowWindows(windowsOnDesktop[GetCurrentDesktop()].table);
											  SystemParametersInfo(SPI_SETDESKWALLPAPER, _tcslen(windowsOnDesktop[GetCurrentDesktop()].szWallpaper), windowsOnDesktop[GetCurrentDesktop()].szWallpaper, 0);
										  }
										  break;
					   }
					   case CMD_PLUGIN:
					   {
										  DialogBox(hInstance, MAKEINTRESOURCE(IDD_SELECT_PLUGIN), hwnd, DlgPluginProc);
										  break;
					   }
					   case CMD_ABOUT:
					   {
										 DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG_ABOUT), hwnd, DlgAboutProc);
										 break;
					   }
					   case CMD_QUIT:
					   {
										SendMessage(hwnd, WM_CLOSE, 0, 0);
										break;
					   }
					   }
					   return 0;
	}
	case WM_QUERYENDSESSION:
	{
							   //SendMessage(hwnd, WM_DESTROY, 0, 0);
							   return TRUE;
	}
	case WM_CLOSE: // catch ALT+F4 to prevent hide tray icon
	{
					   TCHAR szAppName[MAX_PATH];
					   LoadString(hInstance, IDS_APP_NAME, (TCHAR*)szAppName, sizeof(szAppName) / sizeof(TCHAR));

					   TCHAR szExitQuestion[MAX_PATH];
					   LoadString(hInstance, IDS_EXIT_QUESTION, (TCHAR*)szExitQuestion, sizeof(szExitQuestion) / sizeof(TCHAR));
					   if (MessageBox(NULL, szExitQuestion, szAppName, MB_YESNO) == IDYES)
					   {
						   SendMessage(hwnd, WM_DESTROY, 0, 0);
					   }
					   return 0;
	}
	case WM_ENDSESSION:
	case WM_DESTROY:
	{
					   for (int i = 0; i < DESKTOPS; i++)
					   {
						   memset(szWallpaper, 0, sizeof (szWallpaper));
						   _stprintf(szWallpaper, szWallpaperTemplate, i);

						   SetInRegistry(TEXT("Software"), VD_MAIN_KEY, NULL, szWallpaper, (VOID*)windowsOnDesktop[i].szWallpaper, _tcslen(windowsOnDesktop[i].szWallpaper));
					   }
					   //Save plugin path in registry
					   SetInRegistry(TEXT("Software"), VD_MAIN_KEY, NULL, VD_PLUGIN_PATH_KEY, (VOID*)g_PluginUI.GetFullPath(), _tcslen(g_PluginUI.GetFullPath()));

					   tray->Hide();

					   SystemParametersInfo(SPI_SETDESKWALLPAPER, _tcslen(szOrginalWallpaper), szOrginalWallpaper, 0);

					   for (int i = 0; i < DESKTOPS; i++)
					   {
						   SetCurrentDesktop(i);
						   ShowWindows(windowsOnDesktop[GetCurrentDesktop()].table);
					   }

					   SetCurrentDesktop(0);

					   FreeLibrary(hSharedLib);

					   DeleteCriticalSection(&s_criticalSection);

					   PostQuitMessage(0);
					   return 0;
	}
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
