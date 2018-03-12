#include "stdafx.h"
#include "conditioninterface.h"
#include "libwfp/internal/conditionassembler.h"
#include <sstream>
#include <stdexcept>
#include <iphlpapi.h>

using ConditionAssembler = ::wfp::internal::ConditionAssembler;

namespace
{

UINT64 LuidFromAlias(const std::wstring &interfaceAlias)
{
	NET_LUID luid;

	if (0 != ConvertInterfaceAliasToLuid(interfaceAlias.c_str(), &luid))
	{
		throw std::runtime_error("Unable to derive interface LUID from interface alias");
	}

	return luid.Value;
}

UINT64 LuidFromName(const std::wstring &interfaceName)
{
	NET_LUID luid;

	if (0 != ConvertInterfaceNameToLuidW(interfaceName.c_str(), &luid))
	{
		throw std::runtime_error("Unable to derive interface LUID from interface name");
	}

	return luid.Value;
}

} // anonymous namespace

namespace wfp::conditions {

ConditionInterface::ConditionInterface(UINT32 interfaceIndex, const IStrictComparison &comparison)
	: m_interfaceIndex(interfaceIndex)
	, m_interfaceLuid(0)
	, m_identifierType(IdentifierType::Index)
	, m_comparison(comparison)
{
	m_assembled = ConditionAssembler::Uint32(identifier(), m_comparison.op(), m_interfaceIndex);
}

ConditionInterface::ConditionInterface(UINT64 interfaceLuid, const IStrictComparison &comparison)
	: m_interfaceIndex(0)
	, m_interfaceLuid(interfaceLuid)
	, m_identifierType(IdentifierType::Luid)
	, m_comparison(comparison)
{
	m_assembled = ConditionAssembler::Uint64(identifier(), m_comparison.op(), m_interfaceLuid);
}

ConditionInterface::ConditionInterface(const std::wstring &interfaceIdentifier, IdentifierType type, const IStrictComparison &comparison)
	: m_interfaceIndex(0)
	, m_interfaceLuid(0)
	, m_interfaceIdentifier(interfaceIdentifier)
	, m_identifierType(type)
	, m_comparison(comparison)
{
	uint64_t luid;

	switch (m_identifierType)
	{
		case IdentifierType::Alias:
		{
			luid = LuidFromAlias(m_interfaceIdentifier);
			break;
		}
		case IdentifierType::Name:
		{
			luid = LuidFromName(m_interfaceIdentifier);
			break;
		}
		default:
		{
			throw std::logic_error("Missing case handler in switch clause");
		}
	}

	m_assembled = ConditionAssembler::Uint64(identifier(), m_comparison.op(), luid);
}

//static
ConditionInterface *ConditionInterface::Index(uint32_t interfaceIndex, const IStrictComparison &comparison)
{
	return new ConditionInterface(interfaceIndex, comparison);
}

//static
ConditionInterface *ConditionInterface::Luid(uint64_t interfaceLuid, const IStrictComparison &comparison)
{
	return new ConditionInterface(interfaceLuid, comparison);
}

//static
ConditionInterface *ConditionInterface::Alias(const std::wstring &interfaceAlias, const IStrictComparison &comparison)
{
	return new ConditionInterface(interfaceAlias, IdentifierType::Alias, comparison);
}

//static
ConditionInterface *ConditionInterface::Name(const std::wstring &interfaceName, const IStrictComparison &comparison)
{
	return new ConditionInterface(interfaceName, IdentifierType::Name, comparison);
}

std::wstring ConditionInterface::toString() const
{
	std::wstringstream ss;

	switch (m_identifierType)
	{
		case IdentifierType::Index:
		{
			ss << L"interface index " << m_comparison.toString() << L" " << m_interfaceIndex;
			break;
		}
		case IdentifierType::Luid:
		{
			ss << L"interface LUID " << m_comparison.toString() << L" 0x" << std::hex << m_interfaceLuid;
			break;
		}
		case IdentifierType::Alias:
		{
			ss << L"interface alias " << m_comparison.toString() << L" \"" << m_interfaceIdentifier << L"\"";
			break;
		}
		case IdentifierType::Name:
		{
			ss << L"interface name " << m_comparison.toString() << L" \"" << m_interfaceIdentifier << L"\"";
			break;
		}
		default:
		{
			throw std::logic_error("Missing case handler in switch clause");
		}
	};

	return ss.str();
}

const GUID &ConditionInterface::identifier() const
{
	switch (m_identifierType)
	{
		case IdentifierType::Index:
		{
			return FWPM_CONDITION_INTERFACE_INDEX;
		}
		case IdentifierType::Luid:
		case IdentifierType::Alias:
		case IdentifierType::Name:
		{
			return FWPM_CONDITION_IP_LOCAL_INTERFACE;
		}
		default:
		{
			throw std::logic_error("Missing case handler in switch clause");
		}
	};
}

const FWPM_FILTER_CONDITION0 &ConditionInterface::condition() const
{
	return *reinterpret_cast<FWPM_FILTER_CONDITION0 *>(m_assembled.data());
}

}
