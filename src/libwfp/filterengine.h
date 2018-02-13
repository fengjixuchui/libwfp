#pragma once

#include "stdafx.h"
#include <fwpmtypes.h>
#include <windows.h>
#include <memory>

namespace wfp
{

class FilterEngine
{
	struct ctor_tag { explicit ctor_tag() = default; };

public:

	// Create a session using a default timeout when waiting for the transaction lock.
	// The system default timeout on Windows 10 is 100 minutes.
	static std::unique_ptr<FilterEngine> DynamicSession();
	static std::unique_ptr<FilterEngine> StandardSession();

	// Create a session using a specific timeout when waiting for the transaction lock.
	// The timeout value is specified in milliseconds.
	// Specifying a timeout of INFINITE will cause an indefinite wait.
	// Zero is a reserved value in this context and means "system default timeout".
	static std::unique_ptr<FilterEngine> DynamicSession(uint32_t timeout);
	static std::unique_ptr<FilterEngine> StandardSession(uint32_t timeout);

	// Public but non-invokable
	FilterEngine(bool dynamic, uint32_t timeout, ctor_tag);

	~FilterEngine();

	HANDLE session() const;

private:

	FilterEngine(const FilterEngine &);
	FilterEngine &operator=(const FilterEngine &);

	void new_internal(const FWPM_SESSION0 &sessionInfo);

	HANDLE m_session;
};

} // namespace wfp
