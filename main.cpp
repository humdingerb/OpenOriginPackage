/*
 * Copyright 2019. All rights reserved.
 * Distributed under the terms of the MIT license.
 *
 * Authors:
 *	Humdinger, humdingerb@gmail.com
 */

#include <stdio.h>

#include <Alert.h>
#include <Catalog.h>
#include <Entry.h>
#include <FindDirectory.h>
#include <Node.h>
#include <Path.h>
#include <PathFinder.h>
#include <Roster.h>
#include <String.h>
#include <StringFormat.h>
#include <StringList.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Add-On"

static const int32 kMaxFileCount = 10;
static const int32 kMaxAlertFiles = 40;


extern "C" void
process_refs(entry_ref directoryRef, BMessage* msg, void *)
{
	int refCount;
	entry_ref fileRef;

	// Count number of files to be processed, alert when > kMaxFileCount
	for (refCount = 0; msg->FindRef("refs", refCount, &fileRef) == B_NO_ERROR;
			refCount++) { }

	if (refCount > kMaxFileCount) {
		BString alertText(B_TRANSLATE(
			"You have selected %filecount% files.\n"
			"Do you really want to open all of their packages?"));
		BString count;
		count << refCount;
		alertText.ReplaceFirst("%filecount%", count);

		BAlert* maxAlert = new BAlert(B_TRANSLATE_SYSTEM_NAME("Open Origin Package"),
			alertText, B_TRANSLATE("Open all"), B_TRANSLATE("Cancel"));

		maxAlert->SetShortcut(1, B_ESCAPE);
		if (maxAlert->Go() == 1)
			return;
	}

	// Find origin packages and open them
	BStringList packageList;
	BStringList notInPackageList;
	BEntry fileEntry;
	bool foundSome = false;

	for (refCount = 0; msg->FindRef("refs", refCount, &fileRef) == B_NO_ERROR;
		refCount++)
	{
		fileEntry.SetTo(&fileRef, true);  // traverse symlinks
		BNode fileNode(&fileEntry);
		BString packageName;
		status_t status;

		status = fileNode.ReadAttrString("SYS:PACKAGE_FILE", &packageName);
		if (status != B_OK) {
			char buffer[B_FILE_NAME_LENGTH];
			fileEntry.GetName(buffer);
			BString filename(buffer);
			if (!notInPackageList.HasString(filename))
				notInPackageList.Add(filename);
			continue;
		}

		// Don't open the same package multiple times
		if (packageList.HasString(packageName))
			continue;
		else
			packageList.Add(packageName);

		BPathFinder pathFinder;
		BStringList paths;
		BPath path;
		entry_ref ref;

		status_t error = pathFinder.FindPaths(B_FIND_PATH_PACKAGES_DIRECTORY,
			paths);

		for (int i = 0; i < paths.CountStrings(); ++i) {
			if (error == B_OK && path.SetTo(paths.StringAt(i)) == B_OK
					&& path.Append(packageName.String()) == B_OK) {
				status = get_ref_for_path(path.Path(), &ref);
				if (status == B_OK) {
					be_roster->Launch(&ref);
					foundSome = true;
				}
			}
		}
	}

	// Inform that some of the files were not from a package
	if (!notInPackageList.IsEmpty()) {
		BString alertText;
		int32 count = notInPackageList.CountStrings();
		static BStringFormat textFormat(B_TRANSLATE("{0, plural,"
			"=1{The file '%filename%' does not belong to any package.}"
			"other{These # files do not belong to any package:\n}}"));
		textFormat.Format(alertText, count);

		if (count == 1)
			alertText.ReplaceFirst("%filename%", notInPackageList.First());
		else if	(count < kMaxAlertFiles) {
			BString fileName;
			for (int32 i = 0; i < count; i++) {
				alertText << "\t‣ ";
				alertText << notInPackageList.StringAt(i);
				alertText << "\n";
			}
		} else if (foundSome == true) {
			alertText = B_TRANSLATE(
				"The rest of the files don't belong to any package.");
		} else if (foundSome == false) {
			alertText = B_TRANSLATE(
				"None of these files belong to any package.");
		}

		// wait 0.3 secs + 0.1 sec for every found package
		// to have the BAlert pop up on top of the HaikuDepot window
		if (foundSome)
			snooze(300000 + (count * 100000));

		BAlert* alert = new BAlert(B_TRANSLATE_SYSTEM_NAME("Open Origin Package"),
			alertText, B_TRANSLATE("OK"));

		alert->Go();
	}
}


int
main(int /*argc*/, char **/*argv*/)
{
	fprintf(stderr,
		"'Open Origin Package' can only be used as a Tracker add-on.\n");
	return -1;
}