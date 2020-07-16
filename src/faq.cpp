

#include "platform.h"
#include "debug.h"
#include "nes.h"

#include "frontend.h"
#include "settings.h"
#include "imageDraw.h"
#include "calctype/fonts/consolas_intl/consolas_intl.c"

static unsigned char hashString(const char* str) {
	int size = strlen(str);
	unsigned int ret = 0;
	for (int i = 0; i < size; i++) {
		ret = ((ret << 5) + (ret >> 27)) ^ str[i];
	}
	return (ret & 0xFF00) >> 8;
}

void nes_frontend::getFAQName(char* intoBuffer) {
	const char* romFile = nesSettings.GetContinueFile();
	if (romFile == nullptr) {
		intoBuffer[0] = 0;
		return;
	}

	strcpy(intoBuffer, romFile);
	strcpy(strrchr(intoBuffer, '.'), ".txt");
}

bool nes_frontend::loadFAQ() {
	faqHandle = -1;

	char filename[128];
	char faqFile[128];
	unsigned short wFile[128];
	getFAQName(faqFile);
	faqHash = hashString(faqFile);

	strcpy(filename, "\\\\fls0\\");
	strcat(filename, faqFile);

	Bfile_StrToName_ncpy(wFile, filename, 128);
	faqHandle = Bfile_OpenFile_OS(wFile, READ, 0);
	if (faqHandle < 0) {
		return false;
	}

	return true;
}

static uint32 DrawWarp(uint32 ForHash) {
	LoadVRAM_1();
	if (nesFrontend.GetVRAMHash() == ForHash) {
		return ForHash;
	}
	Bdisp_Fill_VRAM(0, 3);
	DrawFrame(0);
	extern PrizmImage gfx_bg_warp;
	gfx_bg_warp.Draw_Blit(0, 71);
	return nesFrontend.GetVRAMHash();
}

void nes_frontend::viewFAQ() {
	char* textBuffer = (char*)malloc(4096);
	int32 faqSize = Bfile_GetFileSize_OS(faqHandle);

	// restore text offset potentially if in the same faq
	int32 bufferPos = 0;
	if (((nesSettings.faqPosition & 0xFF000000) >> 24) == faqHash) {
		bufferPos = nesSettings.faqPosition & 0x00FFFFFF;
	}
	if (bufferPos > faqSize || bufferPos < 0) {
		bufferPos = 0;
	}

	int32 lastBufferPos = -1;
	int32 readOffset = 0;
	int32 x = 0;
	int32 textSize = 0;
	uint32 warpHash = 0;

	bool bSkipKey = false;
	bool bReading = true;
	bool bJumpAfterRead = false;
	do {
		warpHash = DrawWarp(warpHash);

		// read the from the file if we are requesting a new buffer position
		if (bufferPos != lastBufferPos) {
			textSize = min(4096, faqSize - bufferPos);
			Bfile_ReadFile_OS(faqHandle, textBuffer, textSize, bufferPos);
			lastBufferPos = bufferPos;

			if (bJumpAfterRead) {
				readOffset = 0;
				for (int i = 0; i < textSize-1; i++) {
					if (textBuffer[i] == '\n') {
						readOffset = i+1;
						break;
					}
				}
				bJumpAfterRead = false;
			}
		}

		// draw the current text position
		int curTextPos = readOffset;
		{
			for (int y = 1; y + consolas_intl.height < 216 && curTextPos < textSize; y += consolas_intl.height) {
				const int lineSize = 384 / 5;
				char bufToDraw[lineSize + 2];
				bool skipLine = false;
				if (x) {
					for (int c = 0; c < x; c++) {
						if (textBuffer[curTextPos + c] == '\n') {
							skipLine = true;
						}
					}
				}
				if (!skipLine) {
					strncpy(bufToDraw, textBuffer + curTextPos + x, lineSize);
					bufToDraw[lineSize] = 0;
					for (int c = 0; c < lineSize; c++) {
						if (bufToDraw[c] == '\n') {
							bufToDraw[c] = 0;
							break;
						}
					}

					CalcType_Draw(&consolas_intl, bufToDraw, 2, y, COLOR_WHITE, 0, 0);
				}

				// go to next line
				do {
					curTextPos++;
				} while (curTextPos < textSize && textBuffer[curTextPos - 1] != '\n');
			}
		}

		// move down when we need to
		if (bufferPos + textSize != faqSize && curTextPos >= textSize && readOffset > 2048) {
			bufferPos += 2048;
			readOffset -= 2048;
			bSkipKey = true;
		}

		if (!bSkipKey) {
			// draw progress marker
			int progY = min((bufferPos + readOffset) * 200 / faqSize, 200);
			PrizmImage::Draw_BorderRect(380, progY, 4, 16, 1, COLOR_AQUAMARINE);

			// handle nav
			int key = 0;
			GetKey(&key);
			EnableStatusArea(3);

			int32 bookmark = -1;
			switch (key) {
				case KEY_CHAR_0: bookmark = 0; break;
				case KEY_CHAR_1: bookmark = 1; break;
				case KEY_CHAR_2: bookmark = 2; break;
				case KEY_CHAR_3: bookmark = 3; break;
				case KEY_CHAR_4: bookmark = 4; break;
				case KEY_CHAR_5: bookmark = 5; break;
				case KEY_CHAR_6: bookmark = 6; break;
				case KEY_CHAR_7: bookmark = 7; break;
				case KEY_CHAR_8: bookmark = 8; break;
				case KEY_CHAR_9: bookmark = 9; break;
			}
			if (bookmark >= 0) {
				readOffset = 0;
				bufferPos = faqSize * bookmark / 10;
				bJumpAfterRead = bookmark != 0;
				continue;
			}
			
			switch (key) {
				case KEY_CTRL_UP:
				{
					// go to previous line if possible (twice)
					for (int i = 0; i < 2; i++) {
						if (readOffset) {
							do {
								readOffset--;
							} while (readOffset && textBuffer[readOffset - 1] != '\n');
						}

						if (readOffset == 0 && bufferPos > 0) {
							// read back
							int newPos = max(bufferPos - 2048, 0);
							readOffset = bufferPos - newPos;
							bufferPos = newPos;
							bSkipKey = true;
							break;
						}
					}
					break;
				}
				case KEY_CTRL_DOWN:
				{
					// go to next line (twice)
					x = 0;
					for (int i = 0; i < 2; i++) {
						do {
							readOffset++;
						} while (readOffset < textSize && textBuffer[readOffset - 1] != '\n');
					}
					break;
				}
				case KEY_CTRL_RIGHT:
				{
					for (int i = 0; i < 8; i++) {
						if (x + readOffset < textSize && x < 256) {
							x++;
						}
					}
					break;
				}
				case KEY_CTRL_LEFT:
				{
					for (int i = 0; i < 8; i++) {
						if (x) {
							x--;
						}
					}
					break;
				}
				case KEY_CTRL_OPTN:
				case KEY_CTRL_SHIFT:
				case KEY_CTRL_EXE:
				{
					bReading = false;
					break;
				}
			}
		} else {
			bSkipKey = false;
		}
	} while (bReading);

	if (textBuffer != 0) {
		free(textBuffer);
		textBuffer = 0;
	}
	
	if (faqHandle >= 0) {
		Bfile_CloseFile_OS(faqHandle);
		faqHandle = -1;

		// save the real position in the buffer
		bufferPos += readOffset;

		nesSettings.faqPosition = (faqHash << 24) | (bufferPos & 0x00FFFFFF);
		nesSettings.Save();
	}
}