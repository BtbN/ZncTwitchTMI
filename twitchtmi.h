#pragma once

#include <znc/Modules.h>
#include <znc/Threads.h>
#include <unordered_map>


class TwitchTMIUpdateTimer;

class TwitchTMI : public CModule
{
	friend class TwitchTMIUpdateTimer;
	friend class TwitchTMIJob;

	public:
	MODCONSTRUCTOR(TwitchTMI) { }
	virtual ~TwitchTMI();

	virtual bool OnLoad(const CString &sArgsi, CString &sMessage);
	virtual bool OnBoot();

	virtual void OnClientLogin();
	virtual void OnIRCConnected();

	virtual CModule::EModRet OnUserRaw(CString &sLine);
	virtual CModule::EModRet OnUserJoin(CString &sChannel, CString &sKey);
	virtual CModule::EModRet OnUserPart(CString &sChannel, CString &sMessage);
	virtual CModule::EModRet OnPrivMsg(CNick &nick, CString &sMessage);
	virtual CModule::EModRet OnChanMsg(CNick &nick, CChan &channel, CString &sMessage);

	private:
	TwitchTMIUpdateTimer *timer;
	std::unordered_map<CString, CString, std::hash<std::string>, std::equal_to<std::string> > chanTopics;
};

class TwitchTMIUpdateTimer : public CTimer
{
	friend class TwitchTMI;

	public:
	TwitchTMIUpdateTimer(TwitchTMI *mod);

	private:
	virtual void RunJob();

	private:
	TwitchTMI *mod;
};

class TwitchTMIJob : public CJob
{
	public:
	TwitchTMIJob(TwitchTMI *mod, const CString &channel):mod(mod),channel(channel) {}

	virtual void runThread();
	virtual void runMain();

	private:
	TwitchTMI *mod;
	CString channel;
	CString title;
};
