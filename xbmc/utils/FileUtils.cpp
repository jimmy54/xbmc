#include "FileUtils.h"
#include "guilib/GUIWindowManager.h"
#include "dialogs/GUIDialogYesNo.h"
#include "guilib/GUIKeyboardFactory.h"
#include "utils/log.h"
#include "guilib/LocalizeStrings.h"
#include "JobManager.h"
#include "FileOperationJob.h"
#include "URIUtils.h"
#include "filesystem/MultiPathDirectory.h"
#include <vector>
#include "settings/MediaSourceSettings.h"
#include "Util.h"
#include "StringUtils.h"
#include "URL.h"

using namespace XFILE;
using namespace std;

bool CFileUtils::DeleteItem(const CStdString &strPath, bool force)
{
  CFileItemPtr item(new CFileItem(strPath));
  item->SetPath(strPath);
  item->m_bIsFolder = URIUtils::HasSlashAtEnd(strPath);
  item->Select(true);
  return DeleteItem(item, force);
}

bool CFileUtils::DeleteItem(const CFileItemPtr &item, bool force)
{
  if (!item || item->IsParentFolder())
    return false;

  CGUIDialogYesNo* pDialog = (CGUIDialogYesNo*)g_windowManager.GetWindow(WINDOW_DIALOG_YES_NO);
  if (!force && pDialog)
  {
    pDialog->SetHeading(122);
    pDialog->SetLine(0, 125);
    pDialog->SetLine(1, URIUtils::GetFileName(item->GetPath()));
    pDialog->SetLine(2, "");
    pDialog->DoModal();
    if (!pDialog->IsConfirmed()) return false;
  }

  // Create a temporary item list containing the file/folder for deletion
  CFileItemPtr pItemTemp(new CFileItem(*item));
  pItemTemp->Select(true);
  CFileItemList items;
  items.Add(pItemTemp);

  // grab the real filemanager window, set up the progress bar,
  // and process the delete action
  CFileOperationJob op(CFileOperationJob::ActionDelete, items, "");

  return op.DoWork();
}

bool CFileUtils::RenameFile(const CStdString &strFile)
{
  CStdString strFileAndPath(strFile);
  URIUtils::RemoveSlashAtEnd(strFileAndPath);
  CStdString strFileName = URIUtils::GetFileName(strFileAndPath);
  CStdString strPath = strFile.Left(strFileAndPath.size() - strFileName.size());
  if (CGUIKeyboardFactory::ShowAndGetInput(strFileName, g_localizeStrings.Get(16013), false))
  {
    strPath += strFileName;
    CLog::Log(LOGINFO,"FileUtils: rename %s->%s\n", strFileAndPath.c_str(), strPath.c_str());
    if (URIUtils::IsMultiPath(strFileAndPath))
    { // special case for multipath renames - rename all the paths.
      vector<CStdString> paths;
      CMultiPathDirectory::GetPaths(strFileAndPath, paths);
      bool success = false;
      for (unsigned int i = 0; i < paths.size(); ++i)
      {
        CStdString filePath(paths[i]);
        URIUtils::RemoveSlashAtEnd(filePath);
        URIUtils::GetDirectory(filePath, filePath);
        filePath = URIUtils::AddFileToFolder(filePath, strFileName);
        if (CFile::Rename(paths[i], filePath))
          success = true;
      }
      return success;
    }
    return CFile::Rename(strFileAndPath, strPath);
  }
  return false;
}

bool CFileUtils::RemoteAccessAllowed(const CStdString &strPath)
{
  const unsigned int SourcesSize = 5;
  CStdString SourceNames[] = { "programs", "files", "video", "music", "pictures" };

  string realPath = URIUtils::GetRealPath(strPath);
  // for rar:// and zip:// paths we need to extract the path to the archive
  // instead of using the VFS path
  while (URIUtils::IsInArchive(realPath))
    realPath = CURL(realPath).GetHostName();

  if (StringUtils::StartsWith(realPath, "virtualpath://upnproot/"))
    return true;
  else if (StringUtils::StartsWith(realPath, "musicdb://"))
    return true;
  else if (StringUtils::StartsWith(realPath, "videodb://"))
    return true;
  else if (StringUtils::StartsWith(realPath, "library://video"))
    return true;
  else if (StringUtils::StartsWith(realPath, "sources://video"))
    return true;
  else if (StringUtils::StartsWith(realPath, "special://musicplaylists"))
    return true;
  else if (StringUtils::StartsWith(realPath, "special://profile/playlists"))
    return true;
  else if (StringUtils::StartsWith(realPath, "special://videoplaylists"))
    return true;

  bool isSource;
  for (unsigned int index = 0; index < SourcesSize; index++)
  {
    VECSOURCES* sources = CMediaSourceSettings::Get().GetSources(SourceNames[index]);
    int sourceIndex = CUtil::GetMatchingSource(realPath, *sources, isSource);
    if (sourceIndex >= 0 && sourceIndex < (int)sources->size() && sources->at(sourceIndex).m_iHasLock != 2 && sources->at(sourceIndex).m_allowSharing)
      return true;
  }
  return false;
}