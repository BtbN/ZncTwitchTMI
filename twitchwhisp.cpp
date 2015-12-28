#include <znc/IRCNetwork.h>
#include <znc/IRCSock.h>
#include <znc/User.h>

#include "twitchwhisp.h"


TwitchGroupChat::~TwitchGroupChat()
{

}

bool TwitchGroupChat::OnLoad(const CString& sArgsi, CString& sMessage)
{
	PutIRC("CAP REQ :twitch.tv/commands");
	PutIRC("CAP REQ :twitch.tv/tags");

	return true;
}

void TwitchGroupChat::OnIRCConnected()
{
	PutIRC("CAP REQ :twitch.tv/commands");
	PutIRC("CAP REQ :twitch.tv/tags");
}

bool TwitchGroupChat::OnServerCapAvailable(const CString &sCap)
{
	if(sCap == "twitch.tv/commands" || sCap == "twitch.tv/tags")
		return true;
	return false;
}

CModule::EModRet TwitchGroupChat::OnRawMessage(CMessage &msg)
{
	if(msg.GetCommand().Equals("WHISPER"))
		msg.SetCommand("PRIVMSG");

	CString realNick = msg.GetTag("display-name").Trim_n();
	if(realNick != "")
		msg.GetNick().SetNick(realNick);

	CIRCSock *twnw = GetTwitchNetwork();
	if(twnw)
		twnw->ReadLine(msg.ToString());

	return CModule::CONTINUE;
}

CModule::EModRet TwitchGroupChat::OnUserTextMessage(CTextMessage &msg)
{
	if(msg.GetTarget().Left(1).Equals("#"))
		return CModule::CONTINUE;

	msg.SetText(msg.GetText().insert(0, " ").insert(0, msg.GetTarget()).insert(0, "/w "));
	msg.SetTarget("#jtv");

	return CModule::CONTINUE;
}

CIRCSock *TwitchGroupChat::GetTwitchNetwork()
{
	for(CIRCNetwork *nw: GetUser()->GetNetworks())
	{
		for(CModule *mod: nw->GetModules())
		{
			if(mod->GetModName() == "twitch")
			{
				return nw->GetIRCSock();
			}
		}
	}

	return nullptr;
}

template<> void TModInfo<TwitchGroupChat>(CModInfo &info)
{
	info.SetWikiPage("Twitch");
	info.SetHasArgs(false);
}

NETWORKMODULEDEFS(TwitchGroupChat, "Twitch group chat IRC helper module")
