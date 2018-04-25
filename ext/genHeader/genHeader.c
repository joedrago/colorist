#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void genHeader(FILE * inputFile, FILE * outputFile, const char * namespaceName)
{
    unsigned char * bytes;
    int i, size;

    fseek(inputFile, 0, SEEK_END);
    size = (int)ftell(inputFile);
    fseek(inputFile, 0, SEEK_SET);

    fprintf(outputFile,
        "unsigned int %sBinarySize = %d;\n"
        "unsigned char %sBinaryData[] = {\n",
        namespaceName,
        size,
        namespaceName);

    bytes = malloc(size);
    fread(bytes, 1, size, inputFile);
    for (i=0; i < size; i++) {
        fprintf(outputFile, "0x%2.2x", (int)bytes[i]);
        if (i < size - 1)
            fprintf(outputFile, ",");
        if ((i % 15) == 14)
            fprintf(outputFile, "\n");
    }
    fprintf(outputFile, "\n};\n");
    free(bytes);
}

int main(int argc, char * argv[])
{
    int i;
    const char * inputFilename  = NULL;
    const char * outputFilename = NULL;
    const char * namespaceName  = NULL;
    FILE * inputFile;
    FILE * outputFile;

    for (i=0; i < argc; i++) {
        if (!strcmp(argv[i], "-i") && (i + 1 < argc)) {
            inputFilename = argv[++i];
        } else if (!strcmp(argv[i], "-o") && (i + 1 < argc)) {
            outputFilename = argv[++i];
        } else if (!strcmp(argv[i], "-p") && (i + 1 < argc)) {
            namespaceName = argv[++i];
        }
    }

    if (!inputFilename
        || !outputFilename
        || !namespaceName)
    {
        printf("Syntax: genHeader -i [input binary filename] -o [output header filename] -p [prefix to use]\n");
        return -1;
    }

    inputFile  = fopen(inputFilename, "rb");
    if (!inputFile) {
        printf("genHeader ERROR: Can't open '%s' for read.\n", inputFilename);
        return -1;
    }

    outputFile = fopen(outputFilename, "wb");
    if (!outputFile) {
        fclose(inputFile);
        printf("genHeader ERROR: Can't open '%s' for write.\n", outputFilename);
        return -1;
    }

    genHeader(inputFile, outputFile, namespaceName);
    fclose(inputFile);
    fclose(outputFile);
    return 0;
}
