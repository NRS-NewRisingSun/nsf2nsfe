#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <wchar.h>
#include <windows.h>
#include <fcntl.h>
#define	_O_U16TEXT 0x00020000
#define	_O_U8TEXT 0x00040000

void loadAndProcessFile (const wchar_t* fileName) {
	size_t fileNameLength =wcslen(fileName);
	if (fileNameLength <5 ||
	    towupper(fileName[fileNameLength -5]) !='.' ||
	    towupper(fileName[fileNameLength -4]) !='N' ||
	    towupper(fileName[fileNameLength -3]) !='S' ||
	    towupper(fileName[fileNameLength -2]) !='F' ||
	    towupper(fileName[fileNameLength -1]) !='E') {
		fwprintf(stderr, L"%s: File extension is not .NSFE.", fileName);
		return;
	}
	std::wstring outName(fileName);
	outName.pop_back();
		
	FILE *handle =_wfopen(fileName, L"rb");
	if (handle) {
		fseek(handle, 0, SEEK_END);
		std::vector<uint8_t> data(ftell(handle));
		fseek(handle, 0, SEEK_SET);
		fread(&data[0], 1, data.size(), handle);
		fclose(handle);
		
		if (data.at(0) !='N' || data.at(1) !='S' || data.at(2) !='F' || data.at(3) !='E') {
			fwprintf(stderr, L"%s: Has no NSFE header.", fileName);
			return;
		}
		
		std::vector<uint8_t> nsfHeader(128);
		std::vector<uint8_t> nsfData;
		std::vector<uint8_t> metaData;
		nsfHeader[0x6E] =0xFF;
		nsfHeader[0x6F] =0x40;
		nsfHeader[0x78] =0x1D;
		nsfHeader[0x79] =0x4E;
		
		unsigned int chunkSize =0;
		for (unsigned int i =4; (i +8) <=data.size();) {
			chunkSize =data.at(i +0) | data.at(i +1) <<8 | data.at(i +2) <<16 | data.at(i +3) <<24;
			std::string chunkName(&data[i +4], &data[i +8]);
			uint8_t* chunkData =&data[i +8];
			if (chunkName =="INFO" && chunkSize >=10) {
				nsfHeader[0x00] ='N';
				nsfHeader[0x01] ='E';
				nsfHeader[0x02] ='S';
				nsfHeader[0x03] ='M';
				nsfHeader[0x04] =0x1A;
				nsfHeader[0x05] =0x01;
				nsfHeader[0x06] =chunkData[8];
				nsfHeader[0x07] =chunkData[9] +1;
				nsfHeader[0x08] =chunkData[0];
				nsfHeader[0x09] =chunkData[1];
				nsfHeader[0x0A] =chunkData[2];
				nsfHeader[0x0B] =chunkData[3];
				nsfHeader[0x0C] =chunkData[4];
				nsfHeader[0x0D] =chunkData[5];
				nsfHeader[0x7A] =chunkData[6];
				nsfHeader[0x7B] =chunkData[7];
			} else
			if (chunkName =="RATE" && chunkSize >=2) {
				nsfHeader[0x6E] =chunkData[0];
				nsfHeader[0x6F] =chunkData[1];
				if (chunkSize >=4) {
					nsfHeader[0x78] =chunkData[2];
					nsfHeader[0x79] =chunkData[3];
				}
			} else
			if (chunkName =="NSF2" && chunkSize >=1) {
				nsfHeader[0x05] =0x02;
				nsfHeader[0x7C] =chunkData[0];
			} else
			if (chunkName =="BANK" && chunkSize >0) {
				if (chunkSize >=1) nsfHeader[0x70] =chunkData[0];
				if (chunkSize >=2) nsfHeader[0x71] =chunkData[1];
				if (chunkSize >=3) nsfHeader[0x72] =chunkData[2];
				if (chunkSize >=4) nsfHeader[0x73] =chunkData[3];
				if (chunkSize >=5) nsfHeader[0x74] =chunkData[4];
				if (chunkSize >=6) nsfHeader[0x75] =chunkData[5];
				if (chunkSize >=7) nsfHeader[0x76] =chunkData[6];
				if (chunkSize >=8) nsfHeader[0x77] =chunkData[7];
			} else
			if (chunkName =="DATA") {
				for (int j =0; j <chunkSize; j++) nsfData.push_back(chunkData[j]);
			} else
				for (int j =0; j <(chunkSize +8); j++) metaData.push_back(data.at(i +j));
			
			if (chunkName =="auth") {
				int j =0;
				for (int k =0; k <31 && j <chunkSize && chunkData[j] !='\0'; k++) nsfHeader[0x0E +k] =chunkData[j++]; while (j <chunkSize && chunkData[j] !='\0') j++; j++;
				for (int k =0; k <31 && j <chunkSize && chunkData[j] !='\0'; k++) nsfHeader[0x2E +k] =chunkData[j++]; while (j <chunkSize && chunkData[j] !='\0') j++; j++;
				for (int k =0; k <31 && j <chunkSize && chunkData[j] !='\0'; k++) nsfHeader[0x4E +k] =chunkData[j++]; while (j <chunkSize && chunkData[j] !='\0') j++; j++;
			}
			
			if (chunkName =="NEND") break;
			i +=8 +chunkSize;
		}
		nsfHeader[0x7D] =nsfData.size()      &0xFF;
		nsfHeader[0x7E] =nsfData.size() >>8  &0xFF;
		nsfHeader[0x7F] =nsfData.size() >>16 &0xFF;
		
		if (nsfHeader[0] !='N') {
			fwprintf(stderr, L"%s: No or invalid INFO chunk.", fileName);
			return;
		}
		if (nsfData.size() ==0) {
			fwprintf(stderr, L"%s: No or empty DATA chunk.", fileName);
			return;
		}

		handle =_wfopen(outName.c_str(), L"wb");
		if (handle) {
			fwrite(&nsfHeader[0], 1, nsfHeader.size(), handle);
			fwrite(&nsfData[0],   1, nsfData.size(),   handle);
			fwrite(&metaData[0],  1, metaData.size(),  handle);
			fclose(handle);
		} else {
			_wperror(outName.c_str());
		}
	} else {
		_wperror(fileName);
	}
}

int main (int argc, char **) {
	_setmode(_fileno(stdout), _O_U8TEXT);
	_setmode(_fileno(stderr), _O_U8TEXT);

	LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	for (int arg =1; arg <argc; arg++) {
		WIN32_FIND_DATAW result;
		HANDLE hFind =FindFirstFileW(argv[arg], &result);
		if (hFind ==INVALID_HANDLE_VALUE) {
			fwprintf(stderr, L"%s: No matching files found\n", argv[arg]);
			break;
		}
		do {
			loadAndProcessFile(result.cFileName);
		} while (FindNextFileW(hFind, &result));
		FindClose(hFind);
	}
}
