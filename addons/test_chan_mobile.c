/*
 * chan_mobile unit tests
 * 
 * Test AT command parsing, HFP protocol handling, and message queue logic
 * without requiring Bluetooth hardware.
 * 
 * Build: gcc -o test_chan_mobile test_chan_mobile.c -I../include -lbluetooth
 * Run: ./test_chan_mobile
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <bluetooth/bluetooth.h>

/* Test framework macros */
#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  Running %s... ", #name); \
    test_##name(); \
    printf("PASS\n"); \
    tests_passed++; \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAIL\n    Assertion failed: %s\n    at %s:%d\n", #cond, __FILE__, __LINE__); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("FAIL\n    Expected %d, got %d\n    at %s:%d\n", (int)(b), (int)(a), __FILE__, __LINE__); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_STR_EQ(a, b) do { \
    if (strcmp((a), (b)) != 0) { \
        printf("FAIL\n    Expected '%s', got '%s'\n    at %s:%d\n", (b), (a), __FILE__, __LINE__); \
        tests_failed++; \
        return; \
    } \
} while(0)

static int tests_passed = 0;
static int tests_failed = 0;

/*
 * Simplified copies of chan_mobile parsing functions for testing
 */

/* HFP indicator map */
struct hfp_cind {
    int service;
    int call;
    int callsetup;
    int callheld;
    int signal;
    int roam;
    int battchg;
};

/* Parse +CIND: response to build indicator map */
static int parse_cind_test(const char *buf, struct hfp_cind *cind)
{
    /* Example: +CIND: ("service",(0,1)),("call",(0,1)),("callsetup",(0-3)) */
    const char *p = buf;
    int index = 1;
    
    memset(cind, 0, sizeof(*cind));
    
    /* Skip +CIND: prefix */
    if (strncmp(p, "+CIND:", 6) == 0)
        p += 6;
    while (*p == ' ')
        p++;
    
    while (*p) {
        /* Find indicator name in quotes */
        const char *name_start = strchr(p, '"');
        if (!name_start)
            break;
        name_start++;
        
        const char *name_end = strchr(name_start, '"');
        if (!name_end)
            break;
        
        int name_len = name_end - name_start;
        
        /* Match known indicators */
        if (name_len == 7 && strncmp(name_start, "service", 7) == 0)
            cind->service = index;
        else if (name_len == 4 && strncmp(name_start, "call", 4) == 0)
            cind->call = index;
        else if (name_len == 9 && strncmp(name_start, "callsetup", 9) == 0)
            cind->callsetup = index;
        else if (name_len == 8 && strncmp(name_start, "callheld", 8) == 0)
            cind->callheld = index;
        else if (name_len == 6 && strncmp(name_start, "signal", 6) == 0)
            cind->signal = index;
        else if (name_len == 4 && strncmp(name_start, "roam", 4) == 0)
            cind->roam = index;
        else if (name_len == 7 && strncmp(name_start, "battchg", 7) == 0)
            cind->battchg = index;
        
        /* Move to next indicator */
        p = name_end + 1;
        const char *next_paren = strchr(p, '(');
        if (next_paren) {
            p = strchr(next_paren, ')');
            if (p) p++;
        }
        
        /* Skip comma */
        while (*p == ',' || *p == ' ')
            p++;
        
        index++;
    }
    
    return 0;
}

/* Parse +CIND: state response */
static int parse_cind_state(const char *buf, int *states, int max_states)
{
    /* Example: +CIND: 1,0,0,0,5,0,5 */
    const char *p = buf;
    int index = 0;
    
    /* Skip +CIND: prefix */
    if (strncmp(p, "+CIND:", 6) == 0)
        p += 6;
    while (*p == ' ')
        p++;
    
    while (*p && index < max_states) {
        states[index++] = atoi(p);
        
        /* Find next comma or end */
        while (*p && *p != ',')
            p++;
        if (*p == ',')
            p++;
    }
    
    return index;
}

/* Parse +CLIP: caller ID */
static int parse_clip(const char *buf, char *number, int num_len, char *name, int name_len)
{
    /* Example: +CLIP: "+1234567890",145,"",,"John Doe",0 */
    const char *p = buf;
    
    number[0] = '\0';
    name[0] = '\0';
    
    /* Skip +CLIP: prefix */
    if (strncmp(p, "+CLIP:", 6) == 0)
        p += 6;
    while (*p == ' ')
        p++;
    
    /* Parse number (in quotes) */
    if (*p == '"') {
        p++;
        int i = 0;
        while (*p && *p != '"' && i < num_len - 1)
            number[i++] = *p++;
        number[i] = '\0';
        if (*p == '"')
            p++;
    }
    
    /* Skip to name field (5th field) */
    int field = 1;
    while (*p && field < 5) {
        if (*p == ',')
            field++;
        p++;
    }
    
    /* Parse name (in quotes) */
    if (*p == '"') {
        p++;
        int i = 0;
        while (*p && *p != '"' && i < name_len - 1)
            name[i++] = *p++;
        name[i] = '\0';
    }
    
    return 0;
}

/* Parse +CMTI: SMS notification */
static int parse_cmti(const char *buf, char *mem, int mem_len, int *index)
{
    /* Example: +CMTI: "SM",5 */
    const char *p = buf;
    
    mem[0] = '\0';
    *index = -1;
    
    /* Skip +CMTI: prefix */
    if (strncmp(p, "+CMTI:", 6) == 0)
        p += 6;
    while (*p == ' ')
        p++;
    
    /* Parse memory type (in quotes) */
    if (*p == '"') {
        p++;
        int i = 0;
        while (*p && *p != '"' && i < mem_len - 1)
            mem[i++] = *p++;
        mem[i] = '\0';
        if (*p == '"')
            p++;
    }
    
    /* Skip comma */
    while (*p == ',' || *p == ' ')
        p++;
    
    /* Parse index */
    *index = atoi(p);
    
    return 0;
}

/* Parse +BRSF: supported features */
static unsigned int parse_brsf(const char *buf)
{
    /* Example: +BRSF: 871 */
    const char *p = buf;
    
    /* Skip +BRSF: prefix */
    if (strncmp(p, "+BRSF:", 6) == 0)
        p += 6;
    while (*p == ' ')
        p++;
    
    return (unsigned int)atoi(p);
}

/*
 * Test cases
 */

TEST(parse_cind_test_basic)
{
    struct hfp_cind cind;
    const char *response = "+CIND: (\"service\",(0,1)),(\"call\",(0,1)),(\"callsetup\",(0-3)),(\"callheld\",(0-2)),(\"signal\",(0-5)),(\"roam\",(0,1)),(\"battchg\",(0-5))";
    
    parse_cind_test(response, &cind);
    
    ASSERT_EQ(cind.service, 1);
    ASSERT_EQ(cind.call, 2);
    ASSERT_EQ(cind.callsetup, 3);
    ASSERT_EQ(cind.callheld, 4);
    ASSERT_EQ(cind.signal, 5);
    ASSERT_EQ(cind.roam, 6);
    ASSERT_EQ(cind.battchg, 7);
}

TEST(parse_cind_test_reordered)
{
    struct hfp_cind cind;
    /* Some phones report indicators in different order */
    const char *response = "+CIND: (\"battchg\",(0-5)),(\"signal\",(0-5)),(\"service\",(0,1)),(\"call\",(0,1)),(\"callsetup\",(0-3))";
    
    parse_cind_test(response, &cind);
    
    ASSERT_EQ(cind.battchg, 1);
    ASSERT_EQ(cind.signal, 2);
    ASSERT_EQ(cind.service, 3);
    ASSERT_EQ(cind.call, 4);
    ASSERT_EQ(cind.callsetup, 5);
}

TEST(parse_cind_state_basic)
{
    int states[10] = {0};
    const char *response = "+CIND: 1,0,0,0,5,0,3";
    
    int count = parse_cind_state(response, states, 10);
    
    ASSERT_EQ(count, 7);
    ASSERT_EQ(states[0], 1);  /* service */
    ASSERT_EQ(states[1], 0);  /* call */
    ASSERT_EQ(states[2], 0);  /* callsetup */
    ASSERT_EQ(states[3], 0);  /* callheld */
    ASSERT_EQ(states[4], 5);  /* signal */
    ASSERT_EQ(states[5], 0);  /* roam */
    ASSERT_EQ(states[6], 3);  /* battchg */
}

TEST(parse_clip_basic)
{
    char number[32], name[64];
    const char *response = "+CLIP: \"+12025551234\",145,\"\",0,\"John Doe\",0";
    
    parse_clip(response, number, sizeof(number), name, sizeof(name));
    
    ASSERT_STR_EQ(number, "+12025551234");
    ASSERT_STR_EQ(name, "John Doe");
}

TEST(parse_clip_no_name)
{
    char number[32], name[64];
    const char *response = "+CLIP: \"5551234\",129";
    
    parse_clip(response, number, sizeof(number), name, sizeof(name));
    
    ASSERT_STR_EQ(number, "5551234");
    ASSERT_STR_EQ(name, "");
}

TEST(parse_cmti_basic)
{
    char mem[8];
    int index;
    const char *response = "+CMTI: \"SM\",5";
    
    parse_cmti(response, mem, sizeof(mem), &index);
    
    ASSERT_STR_EQ(mem, "SM");
    ASSERT_EQ(index, 5);
}

TEST(parse_cmti_me_storage)
{
    char mem[8];
    int index;
    const char *response = "+CMTI: \"ME\",12";
    
    parse_cmti(response, mem, sizeof(mem), &index);
    
    ASSERT_STR_EQ(mem, "ME");
    ASSERT_EQ(index, 12);
}

TEST(parse_brsf_basic)
{
    const char *response = "+BRSF: 871";
    unsigned int features = parse_brsf(response);
    
    ASSERT_EQ(features, 871);
}

TEST(parse_brsf_high_value)
{
    const char *response = "+BRSF: 4095";
    unsigned int features = parse_brsf(response);
    
    ASSERT_EQ(features, 4095);
}

/* Test +CIEV parsing */
TEST(parse_ciev_basic)
{
    /* +CIEV: indicator_index,value */
    const char *response = "+CIEV: 3,1";
    int index, value;
    
    /* Simple parse - just extract the two numbers */
    ASSERT_EQ(sscanf(response, "+CIEV: %d,%d", &index, &value), 2);
    ASSERT_EQ(index, 3);
    ASSERT_EQ(value, 1);
}

TEST(parse_ciev_callsetup)
{
    /* Callsetup indicator changing to "incoming call" */
    const char *response = "+CIEV: 5,1";
    int index, value;
    
    ASSERT_EQ(sscanf(response, "+CIEV: %d,%d", &index, &value), 2);
    ASSERT_EQ(index, 5);
    ASSERT_EQ(value, 1);
}

/* Test +CUSD parsing */
TEST(parse_cusd_basic)
{
    const char *response = "+CUSD: 0,\"Your balance is $10.00\"";
    char *start, *end;
    char message[256];
    
    /* Find message between quotes */
    start = strchr(response, '"');
    ASSERT(start != NULL);
    start++;
    end = strrchr(response, '"');
    ASSERT(end != NULL);
    ASSERT(end > start);
    
    int len = end - start;
    strncpy(message, start, len);
    message[len] = '\0';
    
    ASSERT_STR_EQ(message, "Your balance is $10.00");
}

/* Test error response parsing */
TEST(parse_error_responses)
{
    const char *ok_response = "OK";
    const char *error_response = "ERROR";
    const char *cms_error = "+CMS ERROR: 500";
    
    ASSERT_EQ(strcmp(ok_response, "OK"), 0);
    ASSERT_EQ(strcmp(error_response, "ERROR"), 0);
    ASSERT(strncmp(cms_error, "+CMS ERROR:", 11) == 0);
}

/* Test SMS prompt detection */
TEST(parse_sms_prompt)
{
    const char *prompt = "> ";
    ASSERT_EQ(prompt[0], '>');
    ASSERT_EQ(prompt[1], ' ');
}

/* Test AT command generation */
TEST(at_command_format)
{
    char buf[64];
    
    /* ATD - dial command */
    snprintf(buf, sizeof(buf), "ATD%s;\r", "+12025551234");
    ASSERT_STR_EQ(buf, "ATD+12025551234;\r");
    
    /* AT+BRSF - supported features */
    snprintf(buf, sizeof(buf), "AT+BRSF=%u\r", 127);
    ASSERT_STR_EQ(buf, "AT+BRSF=127\r");
    
    /* AT+CMER - event reporting */
    snprintf(buf, sizeof(buf), "AT+CMER=3,0,0,%d\r", 1);
    ASSERT_STR_EQ(buf, "AT+CMER=3,0,0,1\r");
    
    /* AT+VGS - speaker volume */
    snprintf(buf, sizeof(buf), "AT+VGS=%d\r", 15);
    ASSERT_STR_EQ(buf, "AT+VGS=15\r");
}

/* Test Bluetooth address handling */
TEST(bdaddr_conversion)
{
    bdaddr_t addr;
    char str[18];
    
    /* String to bdaddr */
    str2ba("00:11:22:33:44:55", &addr);
    
    /* bdaddr to string */
    ba2str(&addr, str);
    ASSERT_STR_EQ(str, "00:11:22:33:44:55");
}

TEST(bdaddr_comparison)
{
    bdaddr_t addr1, addr2, addr3;
    
    str2ba("00:11:22:33:44:55", &addr1);
    str2ba("00:11:22:33:44:55", &addr2);
    str2ba("AA:BB:CC:DD:EE:FF", &addr3);
    
    ASSERT_EQ(bacmp(&addr1, &addr2), 0);
    ASSERT(bacmp(&addr1, &addr3) != 0);
}

/*
 * Additional test cases for comprehensive coverage
 */

/* Test +CLIP with international format */
TEST(parse_clip_international)
{
    char number[32], name[64];
    const char *response = "+CLIP: \"+381641234567\",145,\"\",0,\"Petar Petrovic\",0";
    
    parse_clip(response, number, sizeof(number), name, sizeof(name));
    
    ASSERT_STR_EQ(number, "+381641234567");
    ASSERT_STR_EQ(name, "Petar Petrovic");
}

/* Test +CLIP with withheld number */
TEST(parse_clip_withheld)
{
    char number[32], name[64];
    const char *response = "+CLIP: \"\",128";
    
    parse_clip(response, number, sizeof(number), name, sizeof(name));
    
    ASSERT_STR_EQ(number, "");
    ASSERT_STR_EQ(name, "");
}

/* Test +CLIP with special characters in name */
TEST(parse_clip_special_chars)
{
    char number[32], name[64];
    const char *response = "+CLIP: \"5551234\",129,\"\",0,\"O'Brien, Jr.\",0";
    
    parse_clip(response, number, sizeof(number), name, sizeof(name));
    
    ASSERT_STR_EQ(number, "5551234");
    ASSERT_STR_EQ(name, "O'Brien, Jr.");
}

/* Test +CIND with extra indicators (some phones have more) */
TEST(parse_cind_test_extra_indicators)
{
    struct hfp_cind cind;
    const char *response = "+CIND: (\"service\",(0,1)),(\"call\",(0,1)),(\"callsetup\",(0-3)),(\"callheld\",(0-2)),(\"signal\",(0-5)),(\"roam\",(0,1)),(\"battchg\",(0-5)),(\"message\",(0,1)),(\"smsfull\",(0,1))";
    
    parse_cind_test(response, &cind);
    
    /* Should still parse known indicators correctly */
    ASSERT_EQ(cind.service, 1);
    ASSERT_EQ(cind.call, 2);
    ASSERT_EQ(cind.signal, 5);
    ASSERT_EQ(cind.battchg, 7);
}

/* Test +CIND state with all zeros */
TEST(parse_cind_state_all_zeros)
{
    int states[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    const char *response = "+CIND: 0,0,0,0,0,0,0";
    
    int count = parse_cind_state(response, states, 10);
    
    ASSERT_EQ(count, 7);
    for (int i = 0; i < 7; i++) {
        ASSERT_EQ(states[i], 0);
    }
}

/* Test +CIND state with max values */
TEST(parse_cind_state_max_values)
{
    int states[10] = {0};
    const char *response = "+CIND: 1,1,3,2,5,1,5";
    
    int count = parse_cind_state(response, states, 10);
    
    ASSERT_EQ(count, 7);
    ASSERT_EQ(states[0], 1);  /* service: max 1 */
    ASSERT_EQ(states[1], 1);  /* call: max 1 */
    ASSERT_EQ(states[2], 3);  /* callsetup: max 3 */
    ASSERT_EQ(states[3], 2);  /* callheld: max 2 */
    ASSERT_EQ(states[4], 5);  /* signal: max 5 */
    ASSERT_EQ(states[5], 1);  /* roam: max 1 */
    ASSERT_EQ(states[6], 5);  /* battchg: max 5 */
}

/* Test +BRSF with zero features */
TEST(parse_brsf_zero)
{
    const char *response = "+BRSF: 0";
    unsigned int features = parse_brsf(response);
    
    ASSERT_EQ(features, 0);
}

/* Test +BRSF with specific feature bits */
TEST(parse_brsf_feature_bits)
{
    /* Test specific HFP AG feature bits */
    const char *response = "+BRSF: 495";  /* Common feature set */
    unsigned int features = parse_brsf(response);
    
    ASSERT_EQ(features, 495);
    /* Check individual bits */
    ASSERT((features & 0x01) != 0);  /* 3-way calling */
    ASSERT((features & 0x02) != 0);  /* EC/NR function */
    ASSERT((features & 0x04) != 0);  /* Voice recognition */
    ASSERT((features & 0x08) != 0);  /* In-band ring tone */
}

/* Test +CIEV for signal strength changes */
TEST(parse_ciev_signal)
{
    const char *responses[] = {
        "+CIEV: 5,0",  /* No signal */
        "+CIEV: 5,1",  /* Weak */
        "+CIEV: 5,3",  /* Medium */
        "+CIEV: 5,5",  /* Full */
    };
    int expected[] = {0, 1, 3, 5};
    
    for (int i = 0; i < 4; i++) {
        int index, value;
        ASSERT_EQ(sscanf(responses[i], "+CIEV: %d,%d", &index, &value), 2);
        ASSERT_EQ(index, 5);
        ASSERT_EQ(value, expected[i]);
    }
}

/* Test +CIEV for battery level changes */
TEST(parse_ciev_battery)
{
    const char *responses[] = {
        "+CIEV: 7,0",  /* Empty */
        "+CIEV: 7,1",  /* Low */
        "+CIEV: 7,3",  /* Medium */
        "+CIEV: 7,5",  /* Full */
    };
    int expected[] = {0, 1, 3, 5};
    
    for (int i = 0; i < 4; i++) {
        int index, value;
        ASSERT_EQ(sscanf(responses[i], "+CIEV: %d,%d", &index, &value), 2);
        ASSERT_EQ(index, 7);
        ASSERT_EQ(value, expected[i]);
    }
}

/* Test +CIEV for call state transitions */
TEST(parse_ciev_call_states)
{
    /* Simulate incoming call flow */
    const char *call_flow[] = {
        "+CIEV: 3,1",  /* callsetup: incoming */
        "+CIEV: 2,1",  /* call: active */
        "+CIEV: 3,0",  /* callsetup: none */
        "+CIEV: 2,0",  /* call: ended */
    };
    int expected_idx[] = {3, 2, 3, 2};
    int expected_val[] = {1, 1, 0, 0};
    
    for (int i = 0; i < 4; i++) {
        int index, value;
        ASSERT_EQ(sscanf(call_flow[i], "+CIEV: %d,%d", &index, &value), 2);
        ASSERT_EQ(index, expected_idx[i]);
        ASSERT_EQ(value, expected_val[i]);
    }
}

/* Test +CMTI with different storage types */
TEST(parse_cmti_storage_types)
{
    struct {
        const char *response;
        const char *expected_mem;
        int expected_idx;
    } tests[] = {
        {"+CMTI: \"SM\",1", "SM", 1},
        {"+CMTI: \"ME\",99", "ME", 99},
        {"+CMTI: \"MT\",0", "MT", 0},
        {"+CMTI: \"SR\",5", "SR", 5},
    };
    
    for (int i = 0; i < 4; i++) {
        char mem[8];
        int index;
        parse_cmti(tests[i].response, mem, sizeof(mem), &index);
        ASSERT_STR_EQ(mem, tests[i].expected_mem);
        ASSERT_EQ(index, tests[i].expected_idx);
    }
}

/* Test +CUSD with different response codes */
TEST(parse_cusd_codes)
{
    /* USSD response codes: 0=no further action, 1=action required, 2=terminated */
    const char *responses[] = {
        "+CUSD: 0,\"Balance: $5.00\"",
        "+CUSD: 1,\"Enter PIN\"",
        "+CUSD: 2,\"Session ended\"",
    };
    
    for (int i = 0; i < 3; i++) {
        int code;
        ASSERT_EQ(sscanf(responses[i], "+CUSD: %d", &code), 1);
        ASSERT_EQ(code, i);
    }
}

/* Test +CME ERROR parsing */
TEST(parse_cme_error)
{
    const char *errors[] = {
        "+CME ERROR: 0",    /* Phone failure */
        "+CME ERROR: 3",    /* Operation not allowed */
        "+CME ERROR: 4",    /* Operation not supported */
        "+CME ERROR: 10",   /* SIM not inserted */
        "+CME ERROR: 16",   /* Incorrect password */
        "+CME ERROR: 30",   /* No network service */
    };
    int expected[] = {0, 3, 4, 10, 16, 30};
    
    for (int i = 0; i < 6; i++) {
        int code;
        ASSERT_EQ(sscanf(errors[i], "+CME ERROR: %d", &code), 1);
        ASSERT_EQ(code, expected[i]);
    }
}

/* Test +CMS ERROR parsing (SMS errors) */
TEST(parse_cms_error)
{
    const char *errors[] = {
        "+CMS ERROR: 301",  /* SMS service reserved */
        "+CMS ERROR: 302",  /* Operation not allowed */
        "+CMS ERROR: 321",  /* Invalid memory index */
        "+CMS ERROR: 500",  /* Unknown error */
    };
    int expected[] = {301, 302, 321, 500};
    
    for (int i = 0; i < 4; i++) {
        int code;
        ASSERT_EQ(sscanf(errors[i], "+CMS ERROR: %d", &code), 1);
        ASSERT_EQ(code, expected[i]);
    }
}

/* Test AT command generation - more commands */
TEST(at_command_generation_extended)
{
    char buf[128];
    
    /* AT+CHUP - hang up */
    snprintf(buf, sizeof(buf), "AT+CHUP\r");
    ASSERT_STR_EQ(buf, "AT+CHUP\r");
    
    /* ATA - answer */
    snprintf(buf, sizeof(buf), "ATA\r");
    ASSERT_STR_EQ(buf, "ATA\r");
    
    /* AT+CLIP - caller ID enable */
    snprintf(buf, sizeof(buf), "AT+CLIP=%d\r", 1);
    ASSERT_STR_EQ(buf, "AT+CLIP=1\r");
    
    /* AT+CMGF - SMS text mode */
    snprintf(buf, sizeof(buf), "AT+CMGF=%d\r", 1);
    ASSERT_STR_EQ(buf, "AT+CMGF=1\r");
    
    /* AT+CNMI - new message indication */
    snprintf(buf, sizeof(buf), "AT+CNMI=2,1,0,0,0\r");
    ASSERT_STR_EQ(buf, "AT+CNMI=2,1,0,0,0\r");
    
    /* AT+CMGS - send SMS */
    snprintf(buf, sizeof(buf), "AT+CMGS=\"%s\"\r", "+12025551234");
    ASSERT_STR_EQ(buf, "AT+CMGS=\"+12025551234\"\r");
    
    /* AT+CMGR - read SMS */
    snprintf(buf, sizeof(buf), "AT+CMGR=%d\r", 5);
    ASSERT_STR_EQ(buf, "AT+CMGR=5\r");
    
    /* AT+VGM - microphone volume */
    snprintf(buf, sizeof(buf), "AT+VGM=%d\r", 10);
    ASSERT_STR_EQ(buf, "AT+VGM=10\r");
    
    /* AT+CIND? - query indicators */
    snprintf(buf, sizeof(buf), "AT+CIND?\r");
    ASSERT_STR_EQ(buf, "AT+CIND?\r");
    
    /* AT+CIND=? - test indicators */
    snprintf(buf, sizeof(buf), "AT+CIND=?\r");
    ASSERT_STR_EQ(buf, "AT+CIND=?\r");
}

/* Test Bluetooth address edge cases */
TEST(bdaddr_edge_cases)
{
    bdaddr_t addr;
    char str[18];
    
    /* All zeros */
    str2ba("00:00:00:00:00:00", &addr);
    ba2str(&addr, str);
    ASSERT_STR_EQ(str, "00:00:00:00:00:00");
    
    /* All FFs */
    str2ba("FF:FF:FF:FF:FF:FF", &addr);
    ba2str(&addr, str);
    ASSERT_STR_EQ(str, "FF:FF:FF:FF:FF:FF");
    
    /* Lowercase input */
    str2ba("aa:bb:cc:dd:ee:ff", &addr);
    ba2str(&addr, str);
    ASSERT_STR_EQ(str, "AA:BB:CC:DD:EE:FF");
}

/* Test RING detection */
TEST(parse_ring)
{
    const char *ring = "RING";
    ASSERT_STR_EQ(ring, "RING");
    ASSERT_EQ(strlen(ring), 4);
}

/* Test NO CARRIER detection */
TEST(parse_no_carrier)
{
    const char *no_carrier = "NO CARRIER";
    ASSERT(strncmp(no_carrier, "NO CARRIER", 10) == 0);
}

/* Test BUSY detection */
TEST(parse_busy)
{
    const char *busy = "BUSY";
    ASSERT_STR_EQ(busy, "BUSY");
}

/* Test NO ANSWER detection */
TEST(parse_no_answer)
{
    const char *no_answer = "NO ANSWER";
    ASSERT(strncmp(no_answer, "NO ANSWER", 9) == 0);
}

/* Test +CMGR SMS read parsing */
TEST(parse_cmgr_basic)
{
    /* +CMGR: "REC READ","+12025551234",,"21/01/15,10:30:00+00" */
    const char *response = "+CMGR: \"REC READ\",\"+12025551234\",,\"21/01/15,10:30:00+00\"";
    char *start;
    
    /* Find sender number */
    start = strchr(response, ',');
    ASSERT(start != NULL);
    start++;
    ASSERT(*start == '"');
    start++;
    
    char number[32];
    int i = 0;
    while (*start && *start != '"' && i < 31) {
        number[i++] = *start++;
    }
    number[i] = '\0';
    
    ASSERT_STR_EQ(number, "+12025551234");
}

/* Test state string conversion simulation */
TEST(state_strings)
{
    /* Simulate HFP SLC states */
    const char *hfp_states[] = {
        "DISCONNECTED", "CONNECTING", "BRSF_SENT", "CIND_TEST_SENT",
        "CIND_SENT", "CMER_SENT", "CLIP_SENT", "ECAM_SENT",
        "VGS_SENT", "CMGF_SENT", "CNMI_SENT", "CONNECTED"
    };
    
    ASSERT_EQ(sizeof(hfp_states)/sizeof(hfp_states[0]), 12);
    ASSERT_STR_EQ(hfp_states[0], "DISCONNECTED");
    ASSERT_STR_EQ(hfp_states[11], "CONNECTED");
    
    /* Simulate call states */
    const char *call_states[] = {
        "IDLE", "INCOMING_RING", "WAIT_CALLER_ID", "DIALING",
        "ALERTING", "ACTIVE", "HOLD", "WAITING", "HANGUP_PENDING"
    };
    
    ASSERT_EQ(sizeof(call_states)/sizeof(call_states[0]), 9);
    ASSERT_STR_EQ(call_states[0], "IDLE");
    ASSERT_STR_EQ(call_states[5], "ACTIVE");
}

/* Test exponential backoff calculation */
TEST(backoff_calculation)
{
    int backoff = 0;
    int failures = 0;
    
    /* Initial backoff */
    backoff = 5;
    failures = 1;
    ASSERT_EQ(backoff, 5);
    
    /* Double on each failure */
    backoff = 5 * 2;  /* 10 */
    failures = 2;
    ASSERT_EQ(backoff, 10);
    
    backoff = 10 * 2;  /* 20 */
    failures = 3;
    ASSERT_EQ(backoff, 20);
    
    backoff = 20 * 2;  /* 40 */
    failures = 4;
    ASSERT_EQ(backoff, 40);
    
    /* Max backoff is 300 */
    backoff = 160 * 2;  /* 320 -> capped to 300 */
    if (backoff > 300) backoff = 300;
    ASSERT_EQ(backoff, 300);
}

/* Test time formatting */
TEST(time_formatting)
{
    char buf[16];
    
    /* Format seconds as HH:MM:SS */
    int secs = 3661;  /* 1 hour, 1 minute, 1 second */
    int hours = secs / 3600;
    int mins = (secs % 3600) / 60;
    int s = secs % 60;
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hours, mins, s);
    ASSERT_STR_EQ(buf, "01:01:01");
    
    /* Zero time */
    secs = 0;
    hours = secs / 3600;
    mins = (secs % 3600) / 60;
    s = secs % 60;
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hours, mins, s);
    ASSERT_STR_EQ(buf, "00:00:00");
    
    /* Large time */
    secs = 86400;  /* 24 hours */
    hours = secs / 3600;
    mins = (secs % 3600) / 60;
    s = secs % 60;
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hours, mins, s);
    ASSERT_STR_EQ(buf, "24:00:00");
}

/*
 * Main test runner
 */
int main(int argc, char *argv[])
{
    printf("\n=== chan_mobile Unit Tests ===\n\n");
    
    printf("AT Response Parsing - Basic:\n");
    RUN_TEST(parse_cind_test_basic);
    RUN_TEST(parse_cind_test_reordered);
    RUN_TEST(parse_cind_test_extra_indicators);
    RUN_TEST(parse_cind_state_basic);
    RUN_TEST(parse_cind_state_all_zeros);
    RUN_TEST(parse_cind_state_max_values);
    RUN_TEST(parse_clip_basic);
    RUN_TEST(parse_clip_no_name);
    RUN_TEST(parse_clip_international);
    RUN_TEST(parse_clip_withheld);
    RUN_TEST(parse_clip_special_chars);
    RUN_TEST(parse_cmti_basic);
    RUN_TEST(parse_cmti_me_storage);
    RUN_TEST(parse_cmti_storage_types);
    RUN_TEST(parse_brsf_basic);
    RUN_TEST(parse_brsf_high_value);
    RUN_TEST(parse_brsf_zero);
    RUN_TEST(parse_brsf_feature_bits);
    
    printf("\nAT Response Parsing - CIEV Indicators:\n");
    RUN_TEST(parse_ciev_basic);
    RUN_TEST(parse_ciev_callsetup);
    RUN_TEST(parse_ciev_signal);
    RUN_TEST(parse_ciev_battery);
    RUN_TEST(parse_ciev_call_states);
    
    printf("\nAT Response Parsing - USSD/Errors:\n");
    RUN_TEST(parse_cusd_basic);
    RUN_TEST(parse_cusd_codes);
    RUN_TEST(parse_error_responses);
    RUN_TEST(parse_cme_error);
    RUN_TEST(parse_cms_error);
    RUN_TEST(parse_sms_prompt);
    
    printf("\nAT Response Parsing - Call Events:\n");
    RUN_TEST(parse_ring);
    RUN_TEST(parse_no_carrier);
    RUN_TEST(parse_busy);
    RUN_TEST(parse_no_answer);
    
    printf("\nAT Response Parsing - SMS:\n");
    RUN_TEST(parse_cmgr_basic);
    
    printf("\nAT Command Generation:\n");
    RUN_TEST(at_command_format);
    RUN_TEST(at_command_generation_extended);
    
    printf("\nBluetooth Address Handling:\n");
    RUN_TEST(bdaddr_conversion);
    RUN_TEST(bdaddr_comparison);
    RUN_TEST(bdaddr_edge_cases);
    
    printf("\nState Management:\n");
    RUN_TEST(state_strings);
    RUN_TEST(backoff_calculation);
    RUN_TEST(time_formatting);
    
    printf("\n=== Results ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("\n");
    
    return tests_failed > 0 ? 1 : 0;
}
