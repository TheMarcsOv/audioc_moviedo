/*******************************************************/
/* audioSimpleArgs.c */
/*******************************************************/

/* parses arguments from command line for audioSimple */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h> 
#include <sys/soundcard.h>


#include "audioSimpleArgs.h"
#include "../lib/configureSndcard.h"   /* enum format */

void args_print(int audioSimpleOperation, int sndCardFormat, int channelNumber, int rate, int vol, int blockSize, char *fileNameStr)
{
    if (audioSimpleOperation == RECORD) { 
        printf ("Recording ");}
    else if (audioSimpleOperation == PLAY) { 
        printf ("Playing ");}

    printf ("File requested \'%s\'\nFormat: ", fileNameStr);
    if (sndCardFormat == AFMT_MU_LAW) {
        printf("UNSIGNED 8 BITS, PCM MU LAW ");}
    else {
        printf("SIGNED 16 BITS, BE ");}
    printf (", number of channels: %d, sampling rate: %d, volume: %d\n", channelNumber, rate, vol);
    printf ("Block size used: %d\n", blockSize);
};


static void printHelp (void)
{
    // 2.2 Jul 2019
    printf ("\naudioSimple v2.2");
    printf ("\naudioSimple record|play [-b(8|16)] [stereo] [-vVOLUME] [-rRATE] [-sBLOCK_SIZE] fileName\n");
}


static void defaultValues (int* bitNumber, int * channelNumber, int *rate, int *vol, int *blockSize)
{
    *bitNumber = 8;
    *channelNumber = 1;
    *rate = 8000;
    *vol = 90;
    *blockSize = 1024;
};


int args_capture(int argc,  char * argv[],
        int *audioSimpleOperation, 
        int *bitNumber,     
        int *channelNumber,      
        int *rate,                
        int *vol,                  
        int *blockSize,             
        char **fileNameStr	    
        )
{
    char optionCharacter;
    int numOfNames=0;
    const int maxFilenamesNumber = 1;
    int index;
    int operationDefined = 0; /* 0 if no operation has been defined, 1 otherwise */

    /*set default values */
    defaultValues (bitNumber, channelNumber, rate, vol, blockSize);

    if (argc==1)
    { 
        printf("\nNeeded at least one operation to perform (play, record).\n");
        printHelp();
        return(EXIT_FAILURE);
    }
    for (index=1; argc>1; ++index, --argc)
    {
        if ( *argv[index] == '-')
        {   
            optionCharacter = *(++argv[index]);
            switch (optionCharacter)
            { 
                case 'b': /* BITS */ 
                    if ( sscanf (++argv[index],"%d", bitNumber) != 1) { 
                        printf ("\n-b must be followed by a number.\n");
                        return(EXIT_FAILURE);
                    }
                    if (  ! ( (*bitNumber != 8) || (*bitNumber != 16) )) {	    
                        printf ("\n-b must be followed by either '8' or '16'.\n");
                        return(EXIT_FAILURE);
                    }
                    break;

                case 'v': /* VOLUME */
                    if ( sscanf (++argv[index],"%d", vol) != 1) { 
                        printf ("\n-v must be followed by a number\n");
                        return(EXIT_FAILURE
                                );
                    }
                    if (  ! ( ( (*vol) >= 0) && ((*vol) <= 100)) ) {	    
                        printf ("\n-v must be followed by a number in the range [0..100]\n");
                        return(EXIT_FAILURE);
                    }
                    break;

                case 'r': /* RATE */ 
                    if ( sscanf (++argv[index],"%d", rate) != 1) { 
                        printf ("\n-r must be followed by a number\n");
                        return(EXIT_FAILURE);
                    }
                    if (! ((*rate  >= 0) && (*rate <= 44100)) ) {	    
                        printf ("\n Rate (-r) must be greater than 0 and lower or equal than 44100\n");
                        return(EXIT_FAILURE);
                    }
                    break;

                case 's': /* BLOCK_SIZE */ 
                    if ( sscanf (++argv[index],"%d", blockSize) != 1) { 
                        printf ("\n-s must be followed by a number\n");
                        return(EXIT_FAILURE);
                    }
                    if (! ((*blockSize  >= 16) && (*blockSize <= 65536)) ) {	    
                        printf ("\n Block size (-r) must be greater or equal than 16 and lower or equal than 65536\n");
                        return(EXIT_FAILURE);
                    }
                    break;



                default:
                    printf ("\nI do not understand -%c\n", optionCharacter);
                    printHelp ();
                    return(EXIT_FAILURE);
            }

        }

        else /* There is a name */
        {

            if ( strcmp ( "stereo", argv[index]) == 0)
                *channelNumber = 2;
            else if (strcmp ("record", argv[index]) == 0) {
                (*audioSimpleOperation) = RECORD;
                operationDefined = 1;
            }
            else if (strcmp ("play", argv[index]) == 0) {
                (*audioSimpleOperation) = PLAY;
                operationDefined = 1;
            }
            else {
                int length;
                length = strlen(argv[index]); /* length EXCLUDING the null byte */
                *fileNameStr = malloc(length + 1);
                strcpy (*fileNameStr, argv[index]);
                numOfNames += 1;

                if  (numOfNames > maxFilenamesNumber) { 
                    printf ("\nToo many parameters\n");
                    printHelp ();
                    return(EXIT_FAILURE);
                }
            }
        }
    }

    if (numOfNames != 1) {
        printf("\nNeed a file name.\n");
        printHelp();
        return(EXIT_FAILURE);
    }

    if (operationDefined == 0) {
        /* no operation has been selected. This is an error */
        printf("\nNeed at least one operation (play, record).\n");
        printHelp();
        return(EXIT_FAILURE);
    }
    return(EXIT_SUCCESS);
};


/* _args_test performs several tests to check that the parsing function 
 * args_capture works as defined by 
 * audioSimple record|play [-b(8|16)] [stereo] [-vVOLUME] [-rRATE] [-sBLOCK_SIZE] fileName 
 * Tests are defined by means of a 'test_vector' array.
 * The function stops with the first discrepancy, or indicates that all tests 
 * have occurred as expected. 
 * It test valid and invalid command line arguments
 * */

/* next declaration is used to prevent gcc warnings on this function being unused */
static void _args_test(void)  __attribute__ ((unused));
static void _args_test(void)
{
    struct test_vector_t {
        char *STRARGS;   /* string passed through command line */
        int STATUS;            /* expected result, success, failure (int)
                    If failure, then the rest of the arguments are not tested*/;
        int OPERATION, FORMAT, CHANNELNUM, RATE, VOL, BLOCKSIZE;
        const char *FILENAME;       
    };

    /* default values */
    enum {DEF_FORMAT = 8, DEF_CHAN = 1, DEF_RATE = 8000, DEF_VOL = 90, DEF_BLOCK = 4096};


    /* "test string", expected status (-1, 0), expected operation, 
     * expected format, etc. */
    struct test_vector_t test_vector[] = {

        {"audioc", EXIT_FAILURE, 0, 0, 0, 0, 0,  0, ""},    /* when passing only 
                                                    "audioc ", I expect argument
                                                    processing to fail, status -1 */

        /* tests with unsuccessful return value (invalid commands), 
         * are NOT checked against further values (operation, format, etc.) */

        /* filename missing */
        {"audioc play", EXIT_FAILURE, 0, 0, 0, 0, 0,  0, ""},
        {"audioc stereo", EXIT_FAILURE, 0, 0, 0, 0, 0,  0, ""},
        {"audioc -b16", EXIT_FAILURE, 0, 0, 0, 0, 0,  0, ""},
        
        /* undefined switch */
        {"audioc -i12 filename", EXIT_FAILURE, 0, 0, 0, 0, 0,  0, ""},
        /* too many filenames */
        {"audioc play filename1 filename2", EXIT_FAILURE, 0, 0, 0, 0, 0,  0, ""},
        
        /* tests with successful return value */
        {"audioc play filename", EXIT_SUCCESS, PLAY,  DEF_FORMAT, DEF_CHAN, DEF_RATE, DEF_VOL, DEF_BLOCK, "filename"},
        {"audioc play stereo filename", EXIT_SUCCESS, PLAY,  DEF_FORMAT, 2, DEF_RATE, DEF_VOL, DEF_BLOCK, "filename"},
        /* set all values */
        {"audioc record stereo -b16 -v87 -r10000 -s2000 filename", EXIT_SUCCESS, RECORD,  16, 2, 10000, 87, 2000, "filename"},
        /* different order */
        {"audioc record -s2000 -b16 -v87 stereo -r10000 filename", EXIT_SUCCESS, RECORD,  16, 2, 10000, 87, 2000, "filename"},
        {"audioc record -r10000 -b16 -v87 -s2000 stereo filename", EXIT_SUCCESS, RECORD,  16, 2, 10000, 87, 2000, "filename"},
        /* no stereo */
        {"audioc record -s2000 -b16 -v87 stereo -r10000 filename", EXIT_SUCCESS, RECORD,  16, 2, 10000, 87, 2000, "filename"},
        {"audioc record -r10000 -b16 -v87 -s2000 stereo filename", EXIT_SUCCESS, RECORD,  16, 2, 10000, 87, 2000, "filename"},

        /* repeat same switch, last value should remain */
        {"audioc play -b16 -b16 -b8 filename", EXIT_SUCCESS, PLAY,  8, DEF_CHAN, DEF_RATE, DEF_VOL, DEF_BLOCK, "filename"},

        /* filename is 'stereo' */
        /* next test is not working, we should need to fix the code to allow a filename 'stereo' */
        /* {"audioc record -r10000 -b16 -v87 -s2000 stereo stereo", EXIT_SUCCESS, RECORD,  16, 1, 10000, 87, 2000, "stereo"}, */
    };


    int tests =  sizeof(test_vector)/sizeof(struct test_vector_t); /* number of tests defined in the test vector */
    int test; /* a single test identifier, [0..tests-1] */
    int argc;
    char *p;
    enum { kMaxArgs = 64 }; 
    char *argv[kMaxArgs];

    /* variables to store values returned by args_capture function */
    int return_status;
        int operation;
    int format;  
    int channel_number;      
    int rate;           
    int vol;                 
    int block_size;
    char *file_name_str;
    char *command_line;

    printf("===========\n");
    printf("STARTING tests. Messages may be printed in screen.");
    printf("Tests stop when the first error is detected (or the tests are succesfully completed)\n\n");

    for (test=0; test < tests; test++) {

        /* convert command_line string into argc/argv format */
        file_name_str = NULL;
        argc = 0;

        /* strtok breaks a string into parts according to a delimiter.
         * But it needs the string to be in data space, and test_vector is
         * code space. So we need to copy it to data space */
        command_line = strdup (test_vector[test].STRARGS);

        p = strtok(command_line, " ");
        while (p && argc < kMaxArgs-1)
        {
            argv[argc++] = p;
            p = strtok(NULL, " ");
        }
        argv[argc] = 0;


        return_status = args_capture(argc, argv,
                &operation, &format, &channel_number, &rate, &vol, &block_size,
                &file_name_str);

        /* check returning valued */
        if (return_status != test_vector[test].STATUS) 
        { 
            printf("Test error, unexpected return status, test %d. Returned %d, expected %d\n.", test, return_status, test_vector[test].STATUS);
            if (file_name_str) free(file_name_str);        
            exit(EXIT_FAILURE);
        }
        /* With the current code, all parsing errors (those with return_status = 
         * EXIT_FAILURE) look the same. We could have included an errno error indicator 
         * to express the error observed (e.g., unrecognized operation, volume 
         * value out of range, etc.) to better check the code */
        /* Only check values for EXIT_SUCCESS argument parsing */
        if (return_status == EXIT_SUCCESS) {
            if (operation != test_vector[test].OPERATION)
            { 
                printf("Test error, unexpected OPERATION, test %d. Returned %d, expected %d\n", test, operation, test_vector[test].OPERATION);
                if (file_name_str) free(file_name_str);        
                exit(EXIT_FAILURE);
            }
            if (format !=  test_vector[test].FORMAT)
            {
                printf("Test error, unexpected FORMAT, test %d. Returned %d, expected %d", test, format,  test_vector[test].FORMAT);
                if (file_name_str) free(file_name_str);        
                exit(EXIT_FAILURE);
            }
            if (channel_number !=  test_vector[test].CHANNELNUM)
            {
                printf("Test error, unexpected CHANNELNUM, test %d. Returned %d, expected %d", test, channel_number,  test_vector[test].CHANNELNUM);
                if (file_name_str) free(file_name_str);        
                exit(EXIT_FAILURE);
            } 
            if (rate !=  test_vector[test].RATE)
            {
                printf("Test error, unexpected RATE, test %d. Returned %d, expected %d", test, rate,  test_vector[test].RATE);
                if (file_name_str) free(file_name_str);        
                exit(EXIT_FAILURE);
            }
            if (vol !=  test_vector[test].VOL)
            {
                printf("Test error, unexpected VOL, test %d. Returned %d, expected %d", test, vol,  test_vector[test].VOL);
                if (file_name_str) free(file_name_str);        
                exit(EXIT_FAILURE);
            }
            if (block_size !=  test_vector[test].BLOCKSIZE)
            {
                printf("Test error, unexpected BLOCK_SIZE, test %d. Returned %d, expected %d", test, block_size,  test_vector[test].BLOCKSIZE);
                if (file_name_str) free(file_name_str);        
                exit(EXIT_FAILURE);
            }
            if (strcmp (file_name_str, test_vector[test].FILENAME) != 0)
            {
                printf("Test error, unexpected FILENAME, test %d. Returned %s, expected %s", test, file_name_str,  test_vector[test].FILENAME);
                if (file_name_str) free(file_name_str);        
                exit(EXIT_FAILURE);
            }
        }

        if (command_line) free(command_line);
        if (file_name_str) free(file_name_str);

    }

    /* Tests passed */
    printf ("\nTests PASSED\n");
}

/* To run tests args, uncomment the following lines, and compile with
 * gcc -o testArgs audioSimpleArgs.c 
 * Then, execute:
 * ./testArgs 
 * (remember to comment again the following lines !) */

// int main(int argc, char *argv[])
// {
//     _args_test();
// }
