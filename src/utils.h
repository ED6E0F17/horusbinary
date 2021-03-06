#include <stdint.h>

char *url_encode( char *str );
void UpdatePayloadLOG( char *payload );
void UploadTelemetryPacket( char *Telemetry );
void base64_encode( uint8_t *data, size_t input_length, size_t *output_length, char *encoded_data );

typedef struct {
	uint8_t data[64];
	uint32_t datalen;
	uint32_t bitlen[2];
	uint32_t state[8];
} SHA256_CTX;

void sha256_init( SHA256_CTX *ctx );
void sha256_transform( SHA256_CTX *ctx, uint8_t *data );
void sha256_update( SHA256_CTX *ctx, uint8_t *data, uint32_t len );
void sha256_final( SHA256_CTX *ctx, uint8_t *hash );
void gray2bin( uint8_t *pos, uint8_t len);

