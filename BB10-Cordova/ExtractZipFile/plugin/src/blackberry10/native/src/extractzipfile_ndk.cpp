/*
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string>
#include <json/reader.h>
#include <json/writer.h>
#include <slog2.h>

// minizip
#include "unzip.h"

#include "extractzipfile_ndk.hpp"
#include "extractzipfile_js.hpp"

namespace webworks {

ExtractZipFileNDK::ExtractZipFileNDK(ExtractZipFileJS *parent) {
	m_pParent = parent;
}

ExtractZipFileNDK::~ExtractZipFileNDK() {
}

// ->extractFile
// Returns a json obejct with "result" set to int ret code
// ret code is < 0 on error.
// and "result_message" a description of the error or success
void ExtractZipFileNDK::extractFile(const std::string& callbackId, const std::string& inputString) {
	#define extractReturn(x,y) \
		while (0) {retval["result"] = x; \
		retval["result_message"] = y; \
		m_pParent->NotifyEvent(callbackId + " " + writer.write(retval));}

	// Tune this and reduce mem usage
	#define EZIPBUFSIZE 1024
	#define MAX_FILENAME 1024


	// Parse the arg string as JSON
	Json::FastWriter writer;
	Json::Reader reader;
	Json::Value root;
	Json::Value retval;
	bool parse = reader.parse(inputString, root);
	if (!parse) {
		extractReturn(-1, "Cannot parse internal JSON object");
		m_pParent->NotifyEvent(callbackId + " " + writer.write(retval));
		return;
	}

	// Perform the zip unpacking 
	unzFile zipFile = unzOpen(root["zip"].asString().c_str());
	if (zipFile == NULL)
		extractReturn(-1, "Failed to open zip file.");

	// get zip metadata
	unz_global_info zipInfo;
	if (unzGetGlobalInfo(zipFile, &zipInfo) != UNZ_OK) {
		unzClose(zipFile);
		extractReturn(-1, "Failed to parse zip metadata.");
	}

	// fixed size buf on stack, WATCH USAGE!
	char fileBuffer[EZIPBUFSIZE];

	int filesExtracted = 0;
	for (int i = 0; i < zipInfo.number_entry; i++) {

		// single file metadata
		unz_file_info fileInfo;
		char filename[MAX_FILENAME + 1];

		// get metadata on specific file
		if (unzGetCurrentFileInfo(
					zipFile,
					&fileInfo,
					filename,
					MAX_FILENAME,
					NULL, 0, NULL, 0) != UNZ_OK) {
			unzClose(zipFile);
			extractReturn(-1, "Failed to parse a file's metadata.");
		}
		filename[MAX_FILENAME] = '\0'; // ensure string termination

		// TODO: Support removing created dirs and files upon any failure
		//       keep vector of filenames

		// Handle Directories
		if (filename[strlen(filename) - 1] == '/') {
			// Directory creation cannot lose data
			// so we do not care if a dir already
			// exists
			mkdir(filename, 0x777);
			// Note: The zip format does store permissions
			// except these are all in platform specific
			// formats. I talked with the guy responsible
			// for window's file navigator's zip lib.
			// His stories have scared me into the thinking
			// that providing a default permission is more
			// user friendly than missing obscure edge cases
			// which could leave files in "magical" states.

		// Handle Files
		} else {
			// TODO: check file existance and respect passed in options

			// Note: This opens zipFile's "current" file
			//       "current" acts as an interator
			if (unzOpenCurrentFile(zipFile) != UNZ_OK) {
				unzClose(zipFile);
				extractReturn(-1, "Failed to extract file");
			}

			// Open destination file in file system
			FILE *destFile = fopen(filename, "wb");
			if (destFile == NULL) {
				unzCloseCurrentFile(zipFile);
				unzClose(zipFile);
				extractReturn(-1, "Failed to open destination file");
			}

			// Ferry data into destination
			int readResult = UNZ_OK; // is 0
			do {
				// Read
				readResult = unzReadCurrentFile(zipFile, fileBuffer, EZIPBUFSIZE);
				if (readResult < 0) {
					unzCloseCurrentFile(zipFile);
					unzClose(zipFile);
					fclose(destFile);
					extractReturn(-1, "Failed to read compressed file");
				}

				// Write
				if (readResult > 0) {
					int writeResult = fwrite(fileBuffer, readResult, 1, destFile);
					if (writeResult != 1) {
						// Note: we asked for the full buffer
						// to be written at once, so the 1
						// return value is not "true" but
						// the number blocks writen
						unzCloseCurrentFile(zipFile);
						unzClose(zipFile);
						fclose(destFile);
						extractReturn(-1, "Failed to write to destination file");
					}
				}
			} while (readResult > 0);

			fclose(destFile);
			filesExtracted++;
		}

		unzCloseCurrentFile(zipFile);

		// Increment "current" file interator if any more files
		unzGoToNextFile(zipFile);
	}

	unzClose(zipFile);

	// Success!
	extractReturn(filesExtracted, "Extraction successful.");
}

} /* namespace webworks */
