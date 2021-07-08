#define _BLOBID_STRING_MAX 20
void blobid2char(spdk_blob_id blobid, char *string);
void char2blobid(char *string, spdk_blob_id *blobid);

void blobid2char(spdk_blob_id blobid, char *string)
{
    sprintf(string, "0x%016" PRIx64, blobid);
}
void char2blobid( char *string,spdk_blob_id *blobid)
{
    *blobid = strtoull(string, NULL, 16);
}