/*
 * Copyright 2019. All rights reserved.
 * Distributed under the terms of the MIT license.
 *
 * Authors:
 *	Humdinger, humdingerb@gmail.com
 *
 * This Tracker add-on looks for the SYS:PACKAGE_FILE attribute of files
 * and opens the packages the files come from.
 *
 * + Files without a SYS:PACKAGE_FILE attribute are listed in an alert.
 * + If more than 'kMaxAlertFiles' are without SYS:PACKAGE_FILE attribute,
 *   these files are not listed individually.
 * + If a group of more than 'kMaxFileCount' files are used with the app, an
 *   alert asks for confirmation.
 * + If the packages of files aren't found, those packages are listed in an
 *   alert.
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
static const int32 kMaxAlertFiles = 30;


extern "C" void
process_refs(entry_ref directoryRef, BMessage* msg, void*)
{
	int refCount;
	entry_ref fileRef;

	// Count number of files to be processed, alert when > kMaxFileCount
	for (refCount = 0; msg->FindRef("refs", refCount, &fileRef) == B_NO_ERROR; refCount++) {
	}

	if (refCount > kMaxFileCount) {
		BString alertText(B_TRANSLATE(
			"You have selected %filecount% files.\n"
			"Do you really want to open all of their packages?"));
		BString count;
		count << refCount;
		alertText.ReplaceFirst("%filecount%", count);

		BAlert* maxAlert = new BAlert(B_TRANSLATE_SYSTEM_NAME("Open Origin Package"), alertText,
			B_TRANSLATE("Open all"), B_TRANSLATE("Cancel"));

		maxAlert->SetShortcut(1, B_ESCAPE);
		if (maxAlert->Go() == 1)
			return;
	}

	// Find origin packages and open them
	BStringList packageList;
	BStringList notInPackageList;
	BStringList packageNotFoundList;
	BEntry fileEntry;
	bool foundSome = false;

	for (refCount = 0; msg->FindRef("refs", refCount, &fileRef) == B_NO_ERROR; refCount++) {
		fileEntry.SetTo(&fileRef, true); // traverse symlinks
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

		status_t error = pathFinder.FindPaths(B_FIND_PATH_PACKAGES_DIRECTORY, paths);
		bool foundInNoLocation = true;
		for (int i = 0; i < paths.CountStrings(); ++i) {
			if (error == B_OK && path.SetTo(paths.StringAt(i)) == B_OK
				&& path.Append(packageName.String()) == B_OK) {
				status = get_ref_for_path(path.Path(), &ref);
				if (status == B_OK) {
					status = be_roster->Launch(&ref);
					if ((status == B_OK) || (status == B_ALREADY_RUNNING)) {
						foundSome = true;
						foundInNoLocation = false;
					}
				}
			}
		}
		if (foundInNoLocation == true) {
			if (!packageNotFoundList.HasString(packageName))
				packageNotFoundList.Add(packageName);
		}
	}

	// Inform that some of the files were not from a package
	int32 count = 0;
	BString alertText;
	if (!notInPackageList.IsEmpty()) {
		count = notInPackageList.CountStrings();
		static BStringFormat textFormat(
			B_TRANSLATE("{0, plural,"
				"=1{The file '%filename%' does not belong to any package.}"
				"other{These # files do not belong to any package:\n}}"));
		textFormat.Format(alertText, count);

		if (count == 1)
			alertText.ReplaceFirst("%filename%", notInPackageList.First());
		else if (count < kMaxAlertFiles) {
			for (int32 i = 0; i < count; i++) {
				alertText << "\t‣ ";
				alertText << notInPackageList.StringAt(i);
				alertText << "\n";
			}
		} else if (foundSome == true)
			alertText = B_TRANSLATE("The rest of the files don't belong to any package.");
		else if (foundSome == false)
			alertText = B_TRANSLATE("None of these files belong to any package.");
	}

	// Inform that some packages were not found
	if (!packageNotFoundList.IsEmpty()) {
		BString notFoundText;
		count = packageNotFoundList.CountStrings();
		static BStringFormat textFormat(
			B_TRANSLATE("{0, plural,"
				"=1{The package '%packagename%' cannot be found. It may have been uninstalled.}"
				"other{The following # packages cannot be found. They may have been "
					"uninstalled.\n}}"));
		textFormat.Format(notFoundText, count);

		if (count == 1)
			notFoundText.ReplaceFirst("%packagename%", packageNotFoundList.First());
		else if (count < kMaxAlertFiles) {
			for (int32 i = 0; i < count; i++) {
				notFoundText << "\t‣ ";
				notFoundText << packageNotFoundList.StringAt(i);
				notFoundText << "\n";
			}
		} else if (foundSome == true) {
			notFoundText = B_TRANSLATE(
				"The packages of rest of these files couldn't be found. They "
				"may have been uninstalled.");
		} else if (foundSome == false) {
			notFoundText = B_TRANSLATE(
				"None of the packages of these files could be found. They may "
				"have been uninstalled.");
		}
		if (alertText != "")
			alertText << "\n\n";
		alertText << notFoundText;
	}
	if (!notInPackageList.IsEmpty() || !packageNotFoundList.IsEmpty()) {
		// wait 0.3 secs + 0.1 sec for every found package
		// to have the BAlert pop up on top of the HaikuDepot window
		if (foundSome)
			snooze(300000 + (count * 100000));

		BAlert* alert = new BAlert(
			B_TRANSLATE_SYSTEM_NAME("Open Origin Package"), alertText, B_TRANSLATE("OK"));

		alert->Go();
	}
}


int
main(int /*argc*/, char** /*argv*/)
{
	fprintf(stderr, "'Open Origin Package' can only be used as a Tracker add-on.\n");
	return -1;
}
