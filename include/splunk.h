#ifndef __splunk_hec_h__
#define __splunk_hec_h__
// void sendEvent(String json, String splunkServer, String splunkPort, String splunkToken);


class HEC {
    private:
        String splunkHost, splunkPort, splunkToken;
        String localHost, sourcetype, index;
        String splunkUrl, tokenValue;

    public:
        void init(String splunkHost, String splunkPort, String splunkToken, String host, String sourcetype, String index);
        void init(char *l_Host, char  *l_Port, char *l_Token, char *l_localHost, char *l_sourcetype, char *l_index);
        void sendEvent(String event);
        bool debug = false;
};
#endif