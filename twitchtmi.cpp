#include <znc/main.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/Modules.h>
#include <znc/Nick.h>
#include <znc/Chan.h>

#include "twitchtmi.h"
#include "jload.h"


void TwitchTMI::init()
{
	currentUsers.clear();
}

TwitchTMI::~TwitchTMI()
{

}

bool TwitchTMI::OnLoad(const CString& sArgsi, CString& sMessage)
{
	PutModule("OnLoad");

	for(CChan *chan: GetNetwork()->GetChans())
	{
		addChannel(chan->GetName());
	}

	if(GetNetwork()->IsIRCConnected())
		OnIRCConnected();

	timer = new TwitchTMIUpdateTimer(this);
	AddTimer(timer);

	timer->RunJob();

	return true;
}

bool TwitchTMI::OnBoot()
{
	return true;
}

void TwitchTMI::addChannel(const CString& name)
{
	CString chname = name.substr(1);

	channels.insert(chname);
}

void TwitchTMI::remChannel(const CString& name)
{
	CString chname = name.substr(1);

	channels.erase(chname);
}

void TwitchTMI::OnIRCConnected()
{
	PutIRC("TWITCHCLIENT 3");
}

CModule::EModRet TwitchTMI::OnUserJoin(CString& sChannel, CString& sKey)
{
	addChannel(sChannel);
	return CModule::CONTINUE;
}

CModule::EModRet TwitchTMI::OnUserPart(CString& sChannel, CString& sMessage)
{
	remChannel(sChannel);
	return CModule::CONTINUE;
}

CModule::EModRet TwitchTMI::OnPrivMsg(CNick& nick, CString& sMessage)
{
	if(!nick.GetNick().Equals("jtv", true))
		return CModule::CONTINUE;

	return CModule::HALT;
}

CModule::EModRet TwitchTMI::OnChanMsg(CNick& nick, CChan& channel, CString& sMessage)
{
	if(!nick.GetNick().Equals("jtv", true))
		return CModule::CONTINUE;

	return CModule::HALT;
}



TwitchTMIUpdateTimer::TwitchTMIUpdateTimer(TwitchTMI *tmod)
	:CTimer(tmod, 10, 0, "TwitchTMIUpdateTimer", "Downloads Twitch information")
	,mod(tmod)
{

}

void TwitchTMIUpdateTimer::RunJob()
{
	CString str = getUrl("https://tmi.twitch.tv/group/user/misskaddykins/chatters");

	mod->PutModule(str);
}



template<> void TModInfo<TwitchTMI>(CModInfo &info)
{
	info.SetWikiPage("Twitch");
	info.SetHasArgs(false);
}

NETWORKMODULEDEFS(TwitchTMI, "Twitch IRC helper module")
