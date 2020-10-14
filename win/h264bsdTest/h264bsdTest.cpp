// h264bsdTest.cpp : Defines the entry point for the console application.
//

#include <SDKDDKVer.h>
#include <stdio.h>
#include <tchar.h>
#include <Windows.h>

extern "C"
{
#include "h264bsd_decoder.h"
}

#include <vector>
#include <assert.h>

int _tmain(int argc, _TCHAR* argv[])
{
    storage_t *decoder = NULL;
    char filename[1024];
    sprintf_s(filename, 1024, "%S", argv[1]);
    FILE *input;
    fopen_s(&input, filename, "rb");

    fseek(input, 0L, SEEK_END);
    long fileSize = ftell(input);

    u8 *fileData = (u8*)malloc(fileSize);
    if(fileData == NULL) return 1;

    LARGE_INTEGER frequency_li;
    QueryPerformanceFrequency(&frequency_li);
    double frequency = (double)(frequency_li.QuadPart);

    while(true) {
        fseek(input, 0L, SEEK_SET);
        size_t inputRead = fread(fileData, sizeof(u8), fileSize, input);

        LARGE_INTEGER start;
        QueryPerformanceCounter(&start);

        double numFrames = 0;
        u8* byteStrm = fileData;
        u32 len = fileSize;
        u32 bytesRead = 0;
        u32 status = H264BSD_RDY;

        decoder = h264bsdAlloc();
        status = h264bsdInit(decoder, 0);
        if(status > 0) return 2;

        u32 unitPosition = 0;
        int unitIndex = 0;

        u32 constexpr referenceUnitLengths[] { 6, 28, 8, 55, 246, 9026, 6, 184, 6, 15, 6, 177 };
        int referenceIndex = 0;

/*
NAL     0       6       0
NAL     6       28      0
NAL     34      8       0
NAL     42      55      0
NAL     97      246     0
NAL     343     0       2
NAL     343     9026    1
NAL     9369    6       0
NAL     9375    184     1
NAL     9559    6       0
NAL     9565    15      1
NAL     9580    6       0
NAL     9586    177     1
NAL     9763    6       0
NAL     9769    25      1
NAL     9794    6       0
NAL     9800    159     1
NAL     9959    6       0
NAL     9965    36      1
NAL     10001   6       0
NAL     10007   83      1
NAL     10090   6       0
NAL     10096   23      1
NAL     10119   6       0
NAL     10125   180     1
NAL     10305   6       0
NAL     10311   26      1
NAL     10337   6       0
NAL     10343   199     1
NAL     10542   6       0
NAL     10548   27      1
NAL     10575   6       0
NAL     10581   803     1
NAL     11384   6       0
NAL     11390   2323    1
NAL     13713   6       0
NAL     13719   2953    1
*/
        
        while(len > 0) {

            if(referenceIndex >= std::size(referenceUnitLengths))
                break;
            u8* unitStartCode = byteStrm;
            u32 unitLength = referenceUnitLengths[referenceIndex];

            while(byteStrm[0] == 0x00)
            {
                byteStrm++;
                unitLength--;
            }
            assert(byteStrm[0] == 0x01);
            if(byteStrm[0] == 0x01)
            {
                byteStrm++;
                unitLength--;
            }

            status = h264bsdDecode(decoder, byteStrm, unitLength, 0, &bytesRead);

            if(bytesRead > 0)
            {
                assert(bytesRead == unitLength);
                //byteStrm += unitLength;
                referenceIndex++;
            } else
            {
                byteStrm = unitStartCode;
            }

            //status = h264bsdDecode(decoder, byteStrm, len, 0, &bytesRead);

            printf("NAL\t%u\t%u\t%u\n", unitPosition, bytesRead, status);
            unitPosition += bytesRead;

            if(status == H264BSD_PIC_RDY) {
                ++numFrames;
                u32 picId, isIdrPic, numErrMbs;
                u8* picData = h264bsdNextOutputPicture(decoder, &picId, &isIdrPic, &numErrMbs);
            }

            if(status == H264BSD_ERROR) {
                printf("General Error with %i bytes left\n", len);
            }

            if(status == H264BSD_PARAM_SET_ERROR) {
                printf("Param Set Error with %i bytes left\n", len);
            }

            if(status == H264BSD_MEMALLOC_ERROR) {
                printf("Malloc Error with %i bytes left\n", len);
            }

            byteStrm += bytesRead;
            len-= bytesRead;
            status = H264BSD_RDY;
        }

        h264bsdShutdown(decoder);
        h264bsdFree(decoder);

        LARGE_INTEGER end;
        QueryPerformanceCounter(&end);

        double decodeTime = (double)(end.QuadPart - start.QuadPart) / frequency;

        printf("Decoded %.0f frames in %f seconds (%f fps or %f ms per frame)\n", numFrames, decodeTime, numFrames / decodeTime, decodeTime / numFrames * 1000.0);
    }

	return 0;
}

