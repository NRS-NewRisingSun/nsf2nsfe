#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <wchar.h>
#include <windows.h>
#include <fcntl.h>
#define	_O_U16TEXT 0x00020000
#define	_O_U8TEXT 0x00040000

void writeNSFEChunk (FILE* handle, const char* chunkName, const uint8_t* chunkData, size_t chunkSize) {
	uint8_t temp[4];
	temp[0] =chunkSize >>0 &0xFF;
	temp[1] =chunkSize >>8 &0xFF;
	temp[2] =chunkSize >>16 &0xFF;
	temp[3] =chunkSize >>24 &0xFF;
	fwrite(temp, 1, 4, handle);
	fwrite(chunkName, 1, 4, handle);
	fwrite(chunkData, 1, chunkSize, handle);
}

void loadAndProcessFile (const wchar_t* fileName) {
	size_t fileNameLength =wcslen(fileName);
	if (fileNameLength <4 ||
	    towupper(fileName[fileNameLength -4]) !='.' ||
	    towupper(fileName[fileNameLength -3]) !='N' ||
	    towupper(fileName[fileNameLength -2]) !='S' ||
	    towupper(fileName[fileNameLength -1]) !='F') {
		fwprintf(stderr, L"%s: File extension is not .NSF.", fileName);
		return;
	}
	std::wstring outName(fileName);
	outName.push_back('e');
		
	FILE *handle =_wfopen(fileName, L"rb");
	if (handle) {
		fseek(handle, 0, SEEK_END);
		std::vector<uint8_t> data(ftell(handle));
		fseek(handle, 0, SEEK_SET);
		fread(&data[0], 1, data.size(), handle);
		fclose(handle);
		
		if (data.at(0) !='N' || data.at(1) !='E' || data.at(2) !='S' || data.at(3) !='M' || data.at(4) !=0x1A) {
			fwprintf(stderr, L"%s: Has no NSF header.", fileName);
			return;
		}
		
		handle =_wfopen(outName.c_str(), L"wb");
		if (handle) {
			const char nsfeHeader[5] ="NSFE";
			fwrite(&nsfeHeader[0], 1, 4, handle);
			
			uint8_t infoChunk[10];
			infoChunk[0] =data.at(0x08);
			infoChunk[1] =data.at(0x09);
			infoChunk[2] =data.at(0x0A);
			infoChunk[3] =data.at(0x0B);
			infoChunk[4] =data.at(0x0C);
			infoChunk[5] =data.at(0x0D);
			infoChunk[6] =data.at(0x7A);
			infoChunk[7] =data.at(0x7B);
			infoChunk[8] =data.at(0x06);
			infoChunk[9] =data.at(0x07) -1;
			writeNSFEChunk(handle, "INFO", infoChunk, 10);
			
			uint16_t ntscSpeed =data.at(0x6E) | data.at(0x6F) <<8;
			uint16_t palSpeed  =data.at(0x78) | data.at(0x79) <<8;
			if (~data.at(0x7A) &3 && !(ntscSpeed ==0x40FF || ntscSpeed ==0x411A) || data.at(0x7A) &3 && palSpeed !=0x4E1D) {
				uint8_t rateChunk[4];
				rateChunk[0] =ntscSpeed &0xFF;
				rateChunk[1] =ntscSpeed >>8;
				rateChunk[2] =palSpeed &0xFF;
				rateChunk[3] =palSpeed >>8;
				writeNSFEChunk(handle, "RATE", rateChunk, 4);
			}
			
			if (data.at(0x70) | data.at(0x71) | data.at(0x72) | data.at(0x73) | data.at(0x74) | data.at(0x75) | data.at(0x76) | data.at(0x77)) {
				writeNSFEChunk(handle, "BANK", &data[0x70], 8);
			}
			
			if (data.at(0x05) ==2) {
				writeNSFEChunk(handle, "NSF2", &data[0x7C], 1);
			}
			
			size_t nsfDataLength =data.at(0x7D) | data.at(0x7E) <<8 | data.at(0x7F) <<16;
			if (nsfDataLength ==0) nsfDataLength =data.size() -128;
			if ((nsfDataLength +128) >data.size()) {
				fwprintf(stderr, L"%s: Unexpected end of file", fileName);
				return;				
			}
			size_t metaDataLength =data.size() -nsfDataLength -128;
			
			writeNSFEChunk(handle, "DATA", &data[0x80], nsfDataLength);
			if (metaDataLength) fwrite(&data[0x80 +nsfDataLength], 1, metaDataLength, handle);
			
			bool haveAuth =false;
			bool haveNEND =false;
			unsigned int chunkSize =0;
			uint8_t* metaData =&data[0x80 +nsfDataLength];
			for (unsigned int i =0; (i +8) <=metaDataLength;) {
				chunkSize =metaData[i +0] | metaData[i +1] <<8 | metaData[i +2] <<16 | metaData[i +3] <<24;
				std::string chunkName(&metaData[i +4], &metaData[i +8]);
				uint8_t* chunkData =&metaData[i +0];
				
				if (chunkName =="auth") {
					haveAuth =true;
					break;
				} else
				if (chunkName =="NEND") {
					haveNEND =true;
					break;
				}
				
				i +=8 +chunkSize;
			}

			if (!haveAuth) {
				std::vector<uint8_t> authChunk;
				for (int j =0; j <32 && data[0x0E +j] !='\0'; j++) authChunk.push_back(data[0x0E +j]); authChunk.push_back(0);
				for (int j =0; j <32 && data[0x2E +j] !='\0'; j++) authChunk.push_back(data[0x2E +j]); authChunk.push_back(0);
				for (int j =0; j <32 && data[0x4E +j] !='\0'; j++) authChunk.push_back(data[0x4E +j]); authChunk.push_back(0);
				writeNSFEChunk(handle, "auth", &authChunk[0], authChunk.size());
			}
			if (!haveNEND) writeNSFEChunk(handle, "NEND", &data[0x80], 0);
			
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
