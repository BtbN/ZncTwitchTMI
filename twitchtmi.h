#pragma once

#include <znc/Modules.h>
#include <unordered_set>

typedef std::unordered_set<CString, std::hash<std::string>, std::equal_to<std::string> > CStringSet;
class TwitchTMIUpdateTimer;

class TwitchTMI : public CModule
{
	friend class TwitchTMIUpdateTimer;

	public:
	MODCONSTRUCTOR(TwitchTMI) { init(); }
	virtual ~TwitchTMI();

	virtual bool OnLoad(const CString &sArgsi, CString &sMessage);
	virtual bool OnBoot();

	virtual void OnIRCConnected();

	virtual CModule::EModRet OnUserJoin(CString &sChannel, CString &sKey);
	virtual CModule::EModRet OnUserPart(CString &sChannel, CString &sMessage);
	virtual CModule::EModRet OnPrivMsg(CNick &nick, CString &sMessage);
	virtual CModule::EModRet OnChanMsg(CNick &nick, CChan &channel, CString &sMessage);

	private:
	void init();
	void addChannel(const CString &name);
	void remChannel(const CString &name);

	private:
	CStringSet channels;
	CStringSet currentUsers;
	TwitchTMIUpdateTimer *timer;
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
