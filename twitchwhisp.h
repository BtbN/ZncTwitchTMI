#pragma once

#include <znc/Modules.h>

class TwitchGroupChat : public CModule
{
	public:
	MODCONSTRUCTOR(TwitchGroupChat) {}
	virtual ~TwitchGroupChat();

	virtual bool OnLoad(const CString &sArgsi, CString &sMessage);
	virtual void OnIRCConnected();

	virtual bool OnServerCapAvailable(const CString &sCap);

	virtual CModule::EModRet OnRawMessage(CMessage &msg);
	virtual CModule::EModRet OnUserTextMessage(CTextMessage &msg);

	private:
	CIRCSock *GetTwitchNetwork();
};

