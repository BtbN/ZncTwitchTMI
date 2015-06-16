#include <znc/main.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/Modules.h>
#include <znc/Nick.h>
#include <znc/Chan.h>

#include "twitchtmi.h"
#include "jload.h"



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
			CString chname = ch->GetName().substr(1);

			chanTopics.erase(chname);

			CThreadPool::Get().addJob(new TwitchTMIJob(this, chname));
		}
	}

	return true;
}

bool TwitchTMI::OnBoot()
{
	initCurl();

	timer = new TwitchTMIUpdateTimer(this);
	AddTimer(timer);

	return true;
}

void TwitchTMI::OnClientLogin()
{
	for(const auto &chanTopic: chanTopics)
	{
		std::stringstream ss;
		ss << ":jtv TOPIC #" << chanTopic.first << " :" << chanTopic.second;

		GetClient()->PutClient(ss.str());
	}
}

void TwitchTMI::OnIRCConnected()
{
	chanTopics.clear();
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

	if(!sLine.Left(7).Equals("TOPIC #"))
		return CModule::CONTINUE;

	CString chname = sLine.substr(7);
	chname.Trim();

	auto it = chanTopics.find(chname);

	std::stringstream ss;

	if(it != chanTopics.end())
	{
		ss << ":jtv 332 "
		   << GetUser()->GetNick()
		   << " #"
		   << chname
		   << " :"
		   << it->second;
	}
	else
	{
		ss << ":jtv 331 "
		   << GetUser()->GetNick()
		   << " #"
		   << chname
		   << " :No topic is set";
	}

	PutUser(ss.str());

	return CModule::HALT;
}

CModule::EModRet TwitchTMI::OnUserJoin(CString& sChannel, CString& sKey)
{
	CString chname = sChannel.substr(1);

	chanTopics.erase(chname);

	CThreadPool::Get().addJob(new TwitchTMIJob(this, chname));

	return CModule::CONTINUE;
}

CModule::EModRet TwitchTMI::OnUserPart(CString &sChannel, CString &sMessage)
{
	CString chname = sChannel.substr(1);

	chanTopics.erase(chname);

	return CModule::CONTINUE;
}

CModule::EModRet TwitchTMI::OnPrivMsg(CNick& nick, CString& sMessage)
{
	if(nick.GetNick().Equals("jtv", true))
		return CModule::HALT;

	return CModule::CONTINUE;
}

CModule::EModRet TwitchTMI::OnChanMsg(CNick& nick, CChan& channel, CString& sMessage)
{
	if(nick.GetNick().Equals("jtv", true))
		return CModule::HALT;

	if(sMessage == "FrankerZ" && std::time(nullptr) - lastFrankerZ > 10)
	{
		std::stringstream ss1, ss2;
		CString mynick = GetNetwork()->GetCurNick();

		ss1 << "PRIVMSG " << channel.GetName() << " :FrankerZ";
		ss2 << ":" << mynick << "!" << mynick << "@" << mynick << ".tmi.twitch.tv PRIVMSG " << channel.GetName() << " :FrankerZ";

		PutIRC(ss1.str());
		PutUser(ss2.str());

		lastFrankerZ = std::time(nullptr);
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

	CUtils::PrintMessage(CString("TwitchTMI: Not requesting ") + sCap + " cap");

	return false;
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

	if(!titleVal.isString())
		titleVal = root["title"];

	if(!titleVal.isString())
		return;

	title = titleVal.asString();
}

void TwitchTMIJob::runMain()
{
	if(title.empty())
		return;

	if(!mod->chanTopics[channel].Equals(title, true))
	{
		mod->chanTopics[channel] = title;

		std::stringstream ss;
		ss << ":jtv TOPIC #" << channel << " :" << title;

		mod->PutUser(ss.str());
	}
}


template<> void TModInfo<TwitchTMI>(CModInfo &info)
{
	info.SetWikiPage("Twitch");
	info.SetHasArgs(false);
}

NETWORKMODULEDEFS(TwitchTMI, "Twitch IRC helper module")
