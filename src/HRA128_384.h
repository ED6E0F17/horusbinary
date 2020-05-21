/* H_rows (128,384) Parity matrix for LDPC */

// Choose a value for interleaving:
#define COPRIME         337
// Use same value as Horus Binary

#define DATA_BYTES       16
#define PARITY_BYTES     32
#define CODELENGTH      384

#define NUMBERPARITYBITS 256
#define MAX_ROW_WEIGHT     3

#define NUMBERROWSHCOLS 128	/* DATABITS */
#define MAX_COL_WEIGHT    6

#define DEC_TYPE          0
#define MAX_ITER        100




uint16_t H_rows[256*3] = {
	 1,16,31,46,61,76,91,106,121, 8,23,38,53,68,83,98,113,128,15,30,45,60,75,90,105,120, 7,22,37,52,67,82,97,112,127,14,29,44,59,74,89,104,119, 6,21,36,51,66,81,96,111,126,13,28,43,58,73,88,103,118, 5,20,35,50,65,80,95,110,125,12,27,42,57,72,87,102,117, 4,19,34,49,64,79,94,109,124,11,26,41,56,71,86,101,116, 3,18,33,48,63,78,93,108,123,10,25,40,55,70,85,100,115, 2,17,32,47,62,77,92,107,122, 9,24,39,54,69,84,99,114, 1,16,31,46,61,76,91,106,121, 8,23,38,53,68,83,98,113,128,15,30,45,60,75,90,105,120, 7,22,37,52,67,82,97,112,127,14,29,44,59,74,89,104,119, 6,21,36,51,66,81,96,111,126,13,28,43,58,73,88,103,118, 5,20,35,50,65,80,95,110,125,12,27,42,57,72,87,102,117, 4,19,34,49,64,79,94,109,124,11,26,41,56,71,86,101,116, 3,18,33,48,63,78,93,108,123,10,25,40,55,70,85,100,115, 2,17,32,47,62,77,92,107,122, 9,24,39,54,69,84,99,114,
	 4,73,14,83,24,93,34,103,44,113,54,123,64, 5,74,15,84,25,94,35,104,45,114,55,124,65, 6,75,16,85,26,95,36,105,46,115,56,125,66, 7,76,17,86,27,96,37,106,47,116,57,126,67, 8,77,18,87,28,97,38,107,48,117,58,127,68, 9,78,19,88,29,98,39,108,49,118,59,128,69,10,79,20,89,30,99,40,109,50,119,60, 1,70,11,80,21,90,31,100,41,110,51,120,61, 2,71,12,81,22,91,32,101,42,111,52,121,62, 3,72,13,82,23,92,33,102,43,112,53,122,63, 4,73,14,83,24,93,34,103,44,113,54,123,64, 5,74,15,84,25,94,35,104,45,114,55,124,65, 6,75,16,85,26,95,36,105,46,115,56,125,66, 7,76,17,86,27,96,37,106,47,116,57,126,67, 8,77,18,87,28,97,38,107,48,117,58,127,68, 9,78,19,88,29,98,39,108,49,118,59,128,69,10,79,20,89,30,99,40,109,50,119,60, 1,70,11,80,21,90,31,100,41,110,51,120,61, 2,71,12,81,22,91,32,101,42,111,52,121,62, 3,72,13,82,23,92,33,102,43,112,53,122,63,
	 7,118,101,84,67,50,33,16,127,110,93,76,59,42,25, 8,119,102,85,68,51,34,17,128,111,94,77,60,43,26, 9,120,103,86,69,52,35,18, 1,112,95,78,61,44,27,10,121,104,87,70,53,36,19, 2,113,96,79,62,45,28,11,122,105,88,71,54,37,20, 3,114,97,80,63,46,29,12,123,106,89,72,55,38,21, 4,115,98,81,64,47,30,13,124,107,90,73,56,39,22, 5,116,99,82,65,48,31,14,125,108,91,74,57,40,23, 6,117,100,83,66,49,32,15,126,109,92,75,58,41,24, 7,118,101,84,67,50,33,16,127,110,93,76,59,42,25, 8,119,102,85,68,51,34,17,128,111,94,77,60,43,26, 9,120,103,86,69,52,35,18, 1,112,95,78,61,44,27,10,121,104,87,70,53,36,19, 2,113,96,79,62,45,28,11,122,105,88,71,54,37,20, 3,114,97,80,63,46,29,12,123,106,89,72,55,38,21, 4,115,98,81,64,47,30,13,124,107,90,73,56,39,22, 5,116,99,82,65,48,31,14,125,108,91,74,57,40,23, 6,117,100,83,66,49,32,15,126,109,92,75,58,41,24,
};

uint16_t H_cols[128*6] = {
	 1,112,95,78,61,44,27,10,121,104,87,70,53,36,19, 2,113,96,79,62,45,28,11,122,105,88,71,54,37,20, 3,114,97,80,63,46,29,12,123,106,89,72,55,38,21, 4,115,98,81,64,47,30,13,124,107,90,73,56,39,22, 5,116,99,82,65,48,31,14,125,108,91,74,57,40,23, 6,117,100,83,66,49,32,15,126,109,92,75,58,41,24, 7,118,101,84,67,50,33,16,127,110,93,76,59,42,25, 8,119,102,85,68,51,34,17,128,111,94,77,60,43,26, 9,120,103,86,69,52,35,18,
	129,240,223,206,189,172,155,138,249,232,215,198,181,164,147,130,241,224,207,190,173,156,139,250,233,216,199,182,165,148,131,242,225,208,191,174,157,140,251,234,217,200,183,166,149,132,243,226,209,192,175,158,141,252,235,218,201,184,167,150,133,244,227,210,193,176,159,142,253,236,219,202,185,168,151,134,245,228,211,194,177,160,143,254,237,220,203,186,169,152,135,246,229,212,195,178,161,144,255,238,221,204,187,170,153,136,247,230,213,196,179,162,145,256,239,222,205,188,171,154,137,248,231,214,197,180,163,146,
	90,103,116, 1,14,27,40,53,66,79,92,105,118, 3,16,29,42,55,68,81,94,107,120, 5,18,31,44,57,70,83,96,109,122, 7,20,33,46,59,72,85,98,111,124, 9,22,35,48,61,74,87,100,113,126,11,24,37,50,63,76,89,102,115,128,13,26,39,52,65,78,91,104,117, 2,15,28,41,54,67,80,93,106,119, 4,17,30,43,56,69,82,95,108,121, 6,19,32,45,58,71,84,97,110,123, 8,21,34,47,60,73,86,99,112,125,10,23,36,49,62,75,88,101,114,127,12,25,38,51,64,77,
	218,231,244,129,142,155,168,181,194,207,220,233,246,131,144,157,170,183,196,209,222,235,248,133,146,159,172,185,198,211,224,237,250,135,148,161,174,187,200,213,226,239,252,137,150,163,176,189,202,215,228,241,254,139,152,165,178,191,204,217,230,243,256,141,154,167,180,193,206,219,232,245,130,143,156,169,182,195,208,221,234,247,132,145,158,171,184,197,210,223,236,249,134,147,160,173,186,199,212,225,238,251,136,149,162,175,188,201,214,227,240,253,138,151,164,177,190,203,216,229,242,255,140,153,166,179,192,205,
	39,54,69,84,99,114, 1,16,31,46,61,76,91,106,121, 8,23,38,53,68,83,98,113,128,15,30,45,60,75,90,105,120, 7,22,37,52,67,82,97,112,127,14,29,44,59,74,89,104,119, 6,21,36,51,66,81,96,111,126,13,28,43,58,73,88,103,118, 5,20,35,50,65,80,95,110,125,12,27,42,57,72,87,102,117, 4,19,34,49,64,79,94,109,124,11,26,41,56,71,86,101,116, 3,18,33,48,63,78,93,108,123,10,25,40,55,70,85,100,115, 2,17,32,47,62,77,92,107,122, 9,24,
	167,182,197,212,227,242,129,144,159,174,189,204,219,234,249,136,151,166,181,196,211,226,241,256,143,158,173,188,203,218,233,248,135,150,165,180,195,210,225,240,255,142,157,172,187,202,217,232,247,134,149,164,179,194,209,224,239,254,141,156,171,186,201,216,231,246,133,148,163,178,193,208,223,238,253,140,155,170,185,200,215,230,245,132,147,162,177,192,207,222,237,252,139,154,169,184,199,214,229,244,131,146,161,176,191,206,221,236,251,138,153,168,183,198,213,228,243,130,145,160,175,190,205,220,235,250,137,152,
};

