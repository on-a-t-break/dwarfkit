// TackleBox ships no translation catalogue of its own, so these mirror the
// keys the Anchor native transport looks up, worded for TackleBox. Anything
// missing falls back to the transport's built-in default.
#include <dwarfkit/plugins/wallet/tacklebox.hpp>

namespace dwarfkit {

LocaleDefinitions WalletPluginTackleBox::translations() const {
    static const LocaleDefinitions defs = json::parse(
        R"dk({"en":{
          "login":{
            "title":"Connect with TackleBox",
            "body":"Scan this with TackleBox, or copy the request and paste it into the wallet.",
            "link":"Open TackleBox"
          },
          "transact":{
            "title":"Complete using TackleBox",
            "body":"Open TackleBox on \"{{channelName}}\" to review and approve this transaction.",
            "label":"Sign manually or with another device",
            "link":"Trigger Manually",
            "await":"Waiting for response from TackleBox"
          },
          "error":{
            "expired":"The request expired, please try again.",
            "invalid_response":"Invalid response from TackleBox, must contain link_ch, link_key, link_name and cid flags.",
            "not_completed":"The request was not completed.",
            "cancelled":"The request was cancelled from TackleBox."
          }
        }})dk");
    return defs;
}

}  // namespace dwarfkit
