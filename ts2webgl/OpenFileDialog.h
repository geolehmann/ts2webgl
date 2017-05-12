#pragma once
#include <Windows.h>
#include <Commdlg.h>
#include <algorithm>

std::string filename;

OPENFILENAME    ofn;
char *FilterSpec = "GOCAD ASCII Files(*.*)\0*.*\0"; //Object Files(*.obj)\0*.obj\0Text Files(*.txt)\0*.txt\0All Files
char *Title = "Load GOCAD ASCII File";
char szFileName[MAX_PATH];
char szFileTitle[MAX_PATH];
int             Result;

int openDialog()
{

	/* fill in non-variant fields of OPENFILENAME struct. */
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = GetFocus();
	ofn.lpstrFilter = FilterSpec;
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = szFileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrInitialDir = "."; // Initial directory.
	ofn.lpstrFileTitle = szFileTitle;
	ofn.nMaxFileTitle = MAX_PATH;
	ofn.lpstrTitle = Title;
	ofn.lpstrDefExt = NULL;

	ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

	if (!GetOpenFileName((LPOPENFILENAME)&ofn))
	{
		return -1; // Failed or cancelled
	}
	else
	{
		filename = szFileName;
	}
	return 0;
}