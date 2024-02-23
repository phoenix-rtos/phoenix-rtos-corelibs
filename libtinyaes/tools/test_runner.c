#include <unity_fixture.h>

#define TO_STRING(x)     #x
#define IS_DEFINED(name) (TO_STRING(name)[0] == '1')

#define RUN_TEST_GROUP__IF(label, group) \
    do { \
        if (IS_DEFINED(label)) { \
            RUN_TEST_GROUP(group); \
        } \
    } while (0)

static void execute_tests(void)
{
    RUN_TEST_GROUP__IF(WITH_AES_CCM_S, aes_ccm_s);
    RUN_TEST_GROUP__IF(WITH_AES_CMAC, aes_cmac);
    RUN_TEST_GROUP__IF(WITH_AES_GCM, aes_gcm);
    RUN_TEST_GROUP__IF(WITH_AES_EAX, aes_eax);
    RUN_TEST_GROUP__IF(WITH_AES_KW, aes_kw);
}

int main(int argc, const char* argv[])
{
    return UnityMain(argc, argv, execute_tests);
}
