#include <znc/main.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/Modules.h>
#include <znc/Nick.h>
#include <znc/Chan.h>
#include <znc/IRCSock.h>
#include <znc/version.h>

#include <limits>

#include "twitchtmi.h"
#include "jload.h"

#if (VERSION_MAJOR < 1) || (VERSION_MAJOR == 1 && VERSION_MINOR < 7)
#error This module needs at least ZNC 1.7.0 or later.
#endif

TwitchTMI::~TwitchTMI()
{

}

bool TwitchTMI::OnLoad(const CString& sArgsi, CString& sMessage)
{
	OnBoot();

	if(GetNetwork())
	{
		for(CChan *ch: GetNetwork()->GetChans())
		{
			ch->SetTopic(CString());

			CString chname = ch->GetName().substr(1);
			CThreadPool::Get().addJob(new TwitchTMIJob(this, chname));
		}
	}

	if(GetArgs().Token(0) != "FrankerZ")
		lastFrankerZ = std::numeric_limits<decltype(lastFrankerZ)>::max();

	PutIRC("CAP REQ :twitch.tv/membership");
	PutIRC("CAP REQ :twitch.tv/tags");

	return true;
}

bool TwitchTMI::OnBoot()
{
	initCurl();

	timer = new TwitchTMIUpdateTimer(this);
	AddTimer(timer);

	return true;
}

void TwitchTMI::OnIRCConnected()
{
	PutIRC("CAP REQ :twitch.tv/membership");
	PutIRC("CAP REQ :twitch.tv/tags");
}

CModule::EModRet TwitchTMI::OnUserRaw(CString &sLine)
{
	if(sLine.Left(5).Equals("WHO #"))
		return CModule::HALT;

	if(sLine.Left(5).Equals("AWAY "))
		return CModule::HALT;

	if(sLine.Left(12).Equals("TWITCHCLIENT"))
		return CModule::HALT;

	if(sLine.Left(9).Equals("JTVCLIENT"))
		return CModule::HALT;

	return CModule::CONTINUE;
}

CModule::EModRet TwitchTMI::OnUserJoin(CString& sChannel, CString& sKey)
{
	CString chname = sChannel.substr(1);
	CThreadPool::Get().addJob(new TwitchTMIJob(this, chname));

	return CModule::CONTINUE;
}

CModule::EModRet TwitchTMI::OnPrivMessage(CTextMessage &Message)
{
	if(Message.GetNick().GetNick().Equals("jtv"))
		return CModule::HALT;

	return CModule::CONTINUE;
}

CModule::EModRet TwitchTMI::OnChanMessage(CTextMessage &Message)
{
	if(Message.GetNick().GetNick().Equals("jtv"))
		return CModule::HALT;

	if(Message.GetText() == "FrankerZ" && std::time(nullptr) - lastFrankerZ > 10)
	{
		std::stringstream ss1, ss2;
		CString mynick = GetNetwork()->GetIRCNick().GetNickMask();
		CChan *chan = Message.GetChan();

		ss1 << "PRIVMSG " << chan->GetName() << " :FrankerZ";
		ss2 << ":" << mynick << " PRIVMSG " << chan->GetName() << " :";

		PutIRC(ss1.str());
		CString s2 = ss2.str();

		CThreadPool::Get().addJob(new GenericJob([]() {}, [this, s2, chan]()
		{
			PutUser(s2 + "FrankerZ");

			if(!chan->AutoClearChanBuffer() || !GetNetwork()->IsUserOnline() || chan->IsDetached()) {
				chan->AddBuffer(s2+ "{text}", "FrankerZ");
			}
		}));

		lastFrankerZ = std::time(nullptr);
	}

	CString realNick = Message.GetTag("display-name").Trim_n();

	if(realNick != "")
	{
		Message.GetNick().SetNick(realNick);
	}

	return CModule::CONTINUE;
}

CModule::EModRet TwitchTMI::OnChanActionMessage(CActionMessage &Message)
{
	CString realNick = Message.GetTag("display-name").Trim_n();

	if(realNick != "")
	{
		Message.GetNick().SetNick(realNick);
	}

	return CModule::CONTINUE;
}

bool TwitchTMI::OnServerCapAvailable(const CString &sCap)
{
	if(sCap == "twitch.tv/membership")
	{
		CUtils::PrintMessage("TwitchTMI: Requesting twitch.tv/membership cap");
		return true;
	}
	else if(sCap == "twitch.tv/tags")
	{
		CUtils::PrintMessage("TwitchTMI: Requesting twitch.tv/tags cap");
		return true;
	}

	CUtils::PrintMessage(CString("TwitchTMI: Not requesting ") + sCap + " cap");

	return false;
}

CModule::EModRet TwitchTMI::OnUserTextMessage(CTextMessage &msg)
{
	if(msg.GetTarget().Left(1).Equals("#"))
		return CModule::CONTINUE;

	CIRCNetwork *twnw = GetTwitchGroupNetwork();
	if(!twnw)
		return CModule::CONTINUE;

	msg.SetText(msg.GetText().insert(0, " ").insert(0, msg.GetTarget()).insert(0, "/w "));
	msg.SetTarget("#jtv");

	twnw->PutIRC(msg.ToString());

	return CModule::HALT;
}

CIRCNetwork *TwitchTMI::GetTwitchGroupNetwork()
{
	for(CIRCNetwork *nw: GetUser()->GetNetworks())
	{
		for(CModule *mod: nw->GetModules())
		{
			if(mod->GetModName() == "twitch_group")
				return nw;
		}
	}

	return nullptr;
}



TwitchTMIUpdateTimer::TwitchTMIUpdateTimer(TwitchTMI *tmod)
	:CTimer(tmod, 30, 0, "TwitchTMIUpdateTimer", "Downloads Twitch information")
	,mod(tmod)
{
}

void TwitchTMIUpdateTimer::RunJob()
{
	if(!mod->GetNetwork())
		return;

	for(CChan *chan: mod->GetNetwork()->GetChans())
	{
		CString chname = chan->GetName().substr(1);
		CThreadPool::Get().addJob(new TwitchTMIJob(mod, chname));
	}
}


void TwitchTMIJob::runThread()
{
	std::stringstream ss;
	ss << "https://api.twitch.tv/kraken/channels/" << channel;

	CString url = ss.str();

	Json::Value root = getJsonFromUrl(url.c_str(), "Accept: application/vnd.twitchtv.v3+json");

	if(root.isNull())
	{
		return;
	}

	Json::Value &titleVal = root["status"];
	title = CString();

	if(!titleVal.isString())
		titleVal = root["title"];

	if(!titleVal.isString())
		return;

	title = titleVal.asString();
	title.Trim();
}

void TwitchTMIJob::runMain()
{
	if(title.empty())
		return;

	CChan *chan = mod->GetNetwork()->FindChan(CString("#") + channel);

	if(!chan)
		return;

	if(chan->GetTopic() != title)
	{
		chan->SetTopic(title);
		chan->SetTopicOwner("jtv");
		chan->SetTopicDate((unsigned long)time(nullptr));

		std::stringstream ss;
		ss << ":jtv TOPIC #" << channel << " :" << title;

		mod->PutUser(ss.str());
	}
}


template<> void TModInfo<TwitchTMI>(CModInfo &info)
{
	info.SetWikiPage("Twitch");
	info.SetHasArgs(true);
}

NETWORKMODULEDEFS(TwitchTMI, "Twitch IRC helper module")
