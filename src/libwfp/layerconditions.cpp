#include "stdafx.h"
#include "layerconditions.h"
#include <guiddef.h>
#include <fwpmu.h>
#include <functional>
#include <map>
#include <mutex>
#include <cstring>

namespace wfp
{

namespace
{

// These arrays could be sorted to allow divide-and-conquer lookup, but meh

static const GUID CONDITIONS_INBOUND_IP_PACKET[] =
{
	FWPM_CONDITION_FLAGS,
	FWPM_CONDITION_INTERFACE_INDEX,
	FWPM_CONDITION_INTERFACE_TYPE,
	FWPM_CONDITION_IP_LOCAL_ADDRESS,
	FWPM_CONDITION_IP_LOCAL_ADDRESS_TYPE,
	FWPM_CONDITION_IP_LOCAL_INTERFACE,
	FWPM_CONDITION_IP_REMOTE_ADDRESS,
	FWPM_CONDITION_SUB_INTERFACE_INDEX,
	FWPM_CONDITION_TUNNEL_TYPE
};

static const GUID CONDITIONS_OUTBOUND_IP_PACKET[] =
{
	FWPM_CONDITION_FLAGS,
	FWPM_CONDITION_INTERFACE_INDEX,
	FWPM_CONDITION_INTERFACE_TYPE,
	FWPM_CONDITION_IP_LOCAL_ADDRESS,
	FWPM_CONDITION_IP_LOCAL_ADDRESS_TYPE,
	FWPM_CONDITION_IP_LOCAL_INTERFACE,
	FWPM_CONDITION_IP_REMOTE_ADDRESS,
	FWPM_CONDITION_SUB_INTERFACE_INDEX,
	FWPM_CONDITION_TUNNEL_TYPE
};

static const GUID CONDITIONS_IPFORWARD[] =
{
	FWPM_CONDITION_FLAGS,
	FWPM_CONDITION_DESTINATION_INTERFACE_INDEX,
	FWPM_CONDITION_DESTINATION_SUB_INTERFACE_INDEX,
	FWPM_CONDITION_IP_DESTINATION_ADDRESS,
	FWPM_CONDITION_IP_DESTINATION_ADDRESS_TYPE,
	FWPM_CONDITION_IP_FORWARD_INTERFACE,
	FWPM_CONDITION_IP_LOCAL_INTERFACE,
	FWPM_CONDITION_IP_SOURCE_ADDRESS,
	FWPM_CONDITION_SOURCE_INTERFACE_INDEX,
	FWPM_CONDITION_SOURCE_SUB_INTERFACE_INDEX,

	// Windows 7 and later:
	FWPM_CONDITION_IP_PHYSICAL_ARRIVAL_INTERFACE,
	FWPM_CONDITION_IP_PHYSICAL_NEXTHOP_INTERFACE,
	FWPM_CONDITION_ARRIVAL_INTERFACE_PROFILE_ID,
	FWPM_CONDITION_NEXTHOP_INTERFACE_PROFILE_ID
};

static const GUID CONDITIONS_INBOUND_TRANSPORT[] =
{
	FWPM_CONDITION_FLAGS,
	FWPM_CONDITION_INTERFACE_INDEX,
	FWPM_CONDITION_INTERFACE_TYPE,
	FWPM_CONDITION_IP_LOCAL_ADDRESS,
	FWPM_CONDITION_IP_LOCAL_ADDRESS_TYPE,
	FWPM_CONDITION_IP_LOCAL_INTERFACE,
	FWPM_CONDITION_IP_LOCAL_PORT,
	FWPM_CONDITION_IP_PROTOCOL,
	FWPM_CONDITION_IP_REMOTE_ADDRESS,
	FWPM_CONDITION_IP_REMOTE_PORT,
	FWPM_CONDITION_SUB_INTERFACE_INDEX,
	FWPM_CONDITION_TUNNEL_TYPE,

	// Windows 7 and later:
	FWPM_CONDITION_CURRENT_PROFILE_ID
};

static const GUID CONDITIONS_OUTBOUND_TRANSPORT[] =
{
	FWPM_CONDITION_FLAGS,
	FWPM_CONDITION_INTERFACE_INDEX,
	FWPM_CONDITION_INTERFACE_TYPE,
	FWPM_CONDITION_IP_DESTINATION_ADDRESS_TYPE,
	FWPM_CONDITION_IP_LOCAL_ADDRESS,
	FWPM_CONDITION_IP_LOCAL_ADDRESS_TYPE,
	FWPM_CONDITION_IP_LOCAL_INTERFACE,
	FWPM_CONDITION_IP_LOCAL_PORT,
	FWPM_CONDITION_IP_PROTOCOL,
	FWPM_CONDITION_IP_REMOTE_ADDRESS,
	FWPM_CONDITION_IP_REMOTE_PORT,
	FWPM_CONDITION_SUB_INTERFACE_INDEX,
	FWPM_CONDITION_TUNNEL_TYPE,

	// Windows 7 and later:
	FWPM_CONDITION_CURRENT_PROFILE_ID
};

static const GUID CONDITIONS_STREAM[] =
{
	FWPM_CONDITION_DIRECTION,
	FWPM_CONDITION_FLAGS,
	FWPM_CONDITION_IP_LOCAL_ADDRESS,
	FWPM_CONDITION_IP_LOCAL_ADDRESS_TYPE,
	FWPM_CONDITION_IP_LOCAL_PORT,
	FWPM_CONDITION_IP_REMOTE_ADDRESS,
	FWPM_CONDITION_IP_REMOTE_PORT
};

static const GUID CONDITIONS_DATAGRAM_DATA[] =
{
	FWPM_CONDITION_DIRECTION,
	FWPM_CONDITION_FLAGS,
	FWPM_CONDITION_INTERFACE_INDEX,
	FWPM_CONDITION_INTERFACE_TYPE,
	FWPM_CONDITION_IP_LOCAL_ADDRESS,
	FWPM_CONDITION_IP_LOCAL_ADDRESS_TYPE,
	FWPM_CONDITION_IP_LOCAL_INTERFACE,
	FWPM_CONDITION_IP_LOCAL_PORT,
	FWPM_CONDITION_IP_PROTOCOL,
	FWPM_CONDITION_IP_REMOTE_ADDRESS,
	FWPM_CONDITION_IP_REMOTE_PORT,
	FWPM_CONDITION_SUB_INTERFACE_INDEX,
	FWPM_CONDITION_TUNNEL_TYPE
};

static const GUID CONDITIONS_STREAM_PACKET[] =
{
	// Windows 7 and later:
	FWPM_CONDITION_DIRECTION,
	FWPM_CONDITION_FLAGS,
	FWPM_CONDITION_INTERFACE_INDEX,
	FWPM_CONDITION_INTERFACE_TYPE,
	FWPM_CONDITION_IP_LOCAL_ADDRESS,
	FWPM_CONDITION_IP_LOCAL_INTERFACE,
	FWPM_CONDITION_IP_LOCAL_PORT,
	FWPM_CONDITION_IP_REMOTE_ADDRESS,
	FWPM_CONDITION_IP_REMOTE_PORT,
	FWPM_CONDITION_SUB_INTERFACE_INDEX,
	FWPM_CONDITION_TUNNEL_TYPE
};

static const GUID CONDITIONS_INBOUND_ICMP_ERROR[] =
{
	FWPM_CONDITION_ARRIVAL_INTERFACE_INDEX,
	FWPM_CONDITION_ARRIVAL_INTERFACE_TYPE,
	FWPM_CONDITION_ARRIVAL_SUB_INTERFACE_INDEX,

	// Windows Vista / Windows 7:
	FWPM_CONDITION_SUB_INTERFACE_INDEX,
	FWPM_CONDITION_ARRIVAL_TUNNEL_TYPE,
	FWPM_CONDITION_FLAGS,
	FWPM_CONDITION_ICMP_CODE,
	FWPM_CONDITION_ICMP_TYPE,
	FWPM_CONDITION_EMBEDDED_LOCAL_ADDRESS_TYPE,
	FWPM_CONDITION_EMBEDDED_LOCAL_PORT,
	FWPM_CONDITION_EMBEDDED_PROTOCOL,
	FWPM_CONDITION_EMBEDDED_REMOTE_ADDRESS,
	FWPM_CONDITION_EMBEDDED_REMOTE_PORT,
	FWPM_CONDITION_IP_ARRIVAL_INTERFACE,
	FWPM_CONDITION_IP_LOCAL_ADDRESS,
	FWPM_CONDITION_IP_LOCAL_INTERFACE,
	FWPM_CONDITION_IP_REMOTE_ADDRESS,
	FWPM_CONDITION_LOCAL_INTERFACE_INDEX,

	// Windows Vista / Windows 7:
	FWPM_CONDITION_INTERFACE_INDEX,
	FWPM_CONDITION_LOCAL_INTERFACE_TYPE,

	// Windows Vista / Windows 7:
	FWPM_CONDITION_INTERFACE_TYPE,
	FWPM_CONDITION_LOCAL_TUNNEL_TYPE,

	// Windows Vista / Windows 7:
	FWPM_CONDITION_TUNNEL_TYPE,

	// Windows 7 and later:
	FWPM_CONDITION_ARRIVAL_INTERFACE_PROFILE_ID
};

static const GUID CONDITIONS_OUTBOUND_ICMP_ERROR[] =
{
	FWPM_CONDITION_FLAGS,
	FWPM_CONDITION_ICMP_CODE,
	FWPM_CONDITION_ICMP_TYPE,
	FWPM_CONDITION_INTERFACE_INDEX,
	FWPM_CONDITION_INTERFACE_TYPE,
	FWPM_CONDITION_IP_LOCAL_ADDRESS,
	FWPM_CONDITION_IP_LOCAL_ADDRESS_TYPE,
	FWPM_CONDITION_IP_LOCAL_INTERFACE,
	FWPM_CONDITION_IP_REMOTE_ADDRESS,
	FWPM_CONDITION_SUB_INTERFACE_INDEX,
	FWPM_CONDITION_TUNNEL_TYPE,

	// Windows 7 and later:
	FWPM_CONDITION_NEXTHOP_INTERFACE_PROFILE_ID
};

static const GUID CONDITIONS_ALE_BIND_REDIRECT[] =
{
	// Windows 7 and later:
	FWPM_CONDITION_ALE_APP_ID,
	FWPM_CONDITION_ALE_USER_ID,
	FWPM_CONDITION_FLAGS,
	FWPM_CONDITION_IP_LOCAL_ADDRESS,
	FWPM_CONDITION_IP_LOCAL_ADDRESS_TYPE,
	FWPM_CONDITION_IP_LOCAL_PORT,
	FWPM_CONDITION_IP_PROTOCOL,

	// Windows 8:
	//FWPM_CONDITION_ALE_PACKAGE_ID
};

static const GUID CONDITIONS_ALE_RESOURCE_ASSIGNMENT[] =
{
	FWPM_CONDITION_ALE_APP_ID,
	FWPM_CONDITION_ALE_PROMISCUOUS_MODE,
	FWPM_CONDITION_ALE_USER_ID,
	FWPM_CONDITION_FLAGS,
	FWPM_CONDITION_INTERFACE_TYPE,
	FWPM_CONDITION_IP_LOCAL_ADDRESS,
	FWPM_CONDITION_IP_LOCAL_ADDRESS_TYPE,
	FWPM_CONDITION_IP_LOCAL_INTERFACE,
	FWPM_CONDITION_IP_LOCAL_PORT,
	FWPM_CONDITION_IP_PROTOCOL,
	FWPM_CONDITION_TUNNEL_TYPE,

	// Windows 7 and later:
	FWPM_CONDITION_LOCAL_INTERFACE_PROFILE_ID,
	FWPM_CONDITION_ALE_SIO_FIREWALL_SYSTEM_PORT,

	// Windows 8 :
	//FWPM_CONDITION_ALE_PACKAGE_ID
};

static const GUID CONDITIONS_ALE_RESOURCE_RELEASE[] =
{
	// Windows 7 and later:
	FWPM_CONDITION_ALE_APP_ID,
	FWPM_CONDITION_ALE_USER_ID,
	FWPM_CONDITION_FLAGS,
	FWPM_CONDITION_IP_LOCAL_ADDRESS,
	FWPM_CONDITION_IP_LOCAL_ADDRESS_TYPE,
	FWPM_CONDITION_IP_LOCAL_INTERFACE,
	FWPM_CONDITION_IP_LOCAL_PORT,
	FWPM_CONDITION_IP_PROTOCOL,

	// Windows 8:
	//FWPM_CONDITION_ALE_PACKAGE_ID
};

static const GUID CONDITIONS_ALE_ENDPOINT_CLOSURE[] =
{
	// Windows 7 and later:
	FWPM_CONDITION_ALE_APP_ID,
	FWPM_CONDITION_ALE_USER_ID,
	FWPM_CONDITION_FLAGS,
	FWPM_CONDITION_IP_LOCAL_ADDRESS,
	FWPM_CONDITION_IP_LOCAL_ADDRESS_TYPE,
	FWPM_CONDITION_IP_LOCAL_INTERFACE,
	FWPM_CONDITION_IP_LOCAL_PORT,
	FWPM_CONDITION_IP_PROTOCOL,
	FWPM_CONDITION_IP_REMOTE_ADDRESS,
	FWPM_CONDITION_IP_REMOTE_PORT,

	// Windows 8:
	//FWPM_CONDITION_ALE_PACKAGE_ID
};

static const GUID CONDITIONS_ALE_AUTH_LISTEN[] =
{
	FWPM_CONDITION_ALE_APP_ID,
	FWPM_CONDITION_ALE_USER_ID,
	FWPM_CONDITION_FLAGS,
	FWPM_CONDITION_INTERFACE_TYPE,
	FWPM_CONDITION_IP_LOCAL_ADDRESS,
	FWPM_CONDITION_IP_LOCAL_ADDRESS_TYPE,
	FWPM_CONDITION_IP_LOCAL_INTERFACE,
	FWPM_CONDITION_IP_LOCAL_PORT,
	FWPM_CONDITION_TUNNEL_TYPE,

	// Windows 7 and later:
	FWPM_CONDITION_LOCAL_INTERFACE_PROFILE_ID,
	FWPM_CONDITION_ALE_SIO_FIREWALL_SYSTEM_PORT,

	// Windows 8:
	//FWPM_CONDITION_ALE_PACKAGE_ID
};

static const GUID CONDITIONS_ALE_AUTH_RECV_ACCEPT[] =
{
	FWPM_CONDITION_ALE_APP_ID,
	FWPM_CONDITION_ALE_NAP_CONTEXT,
	FWPM_CONDITION_ALE_REMOTE_MACHINE_ID,
	FWPM_CONDITION_ALE_REMOTE_USER_ID,
	FWPM_CONDITION_ALE_SIO_FIREWALL_SYSTEM_PORT,
	FWPM_CONDITION_ALE_USER_ID,
	FWPM_CONDITION_ARRIVAL_INTERFACE_INDEX,
	FWPM_CONDITION_ARRIVAL_INTERFACE_TYPE,
	FWPM_CONDITION_ARRIVAL_SUB_INTERFACE_INDEX,

	// Windows Vista / Windows 7:
	FWPM_CONDITION_SUB_INTERFACE_INDEX,
	FWPM_CONDITION_ARRIVAL_TUNNEL_TYPE,
	FWPM_CONDITION_FLAGS,
	FWPM_CONDITION_IP_ARRIVAL_INTERFACE,
	FWPM_CONDITION_IP_LOCAL_ADDRESS,
	FWPM_CONDITION_IP_LOCAL_ADDRESS_TYPE,
	FWPM_CONDITION_IP_LOCAL_INTERFACE,
	FWPM_CONDITION_IP_LOCAL_PORT,
	FWPM_CONDITION_IP_PROTOCOL,
	FWPM_CONDITION_IP_REMOTE_ADDRESS,
	FWPM_CONDITION_IP_REMOTE_PORT,
	FWPM_CONDITION_LOCAL_INTERFACE_INDEX,

	// Windows Vista / Windows 7:
	FWPM_CONDITION_INTERFACE_INDEX,
	FWPM_CONDITION_LOCAL_INTERFACE_TYPE,

	// Windows Vista / Windows 7:
	FWPM_CONDITION_INTERFACE_TYPE,
	FWPM_CONDITION_LOCAL_TUNNEL_TYPE,

	// Windows Vista / Windows 7:
	FWPM_CONDITION_TUNNEL_TYPE,

	// Windows 7 and later:
	FWPM_CONDITION_NEXTHOP_SUB_INTERFACE_INDEX,
	FWPM_CONDITION_IP_NEXTHOP_INTERFACE,
	FWPM_CONDITION_NEXTHOP_INTERFACE_TYPE,
	FWPM_CONDITION_NEXTHOP_TUNNEL_TYPE,
	FWPM_CONDITION_NEXTHOP_INTERFACE_INDEX,
	FWPM_CONDITION_ORIGINAL_PROFILE_ID,
	FWPM_CONDITION_CURRENT_PROFILE_ID,
	FWPM_CONDITION_REAUTHORIZE_REASON,
	FWPM_CONDITION_ORIGINAL_ICMP_TYPE,

	// Windows 8:
	//FWPM_CONDITION_ALE_PACKAGE_ID
};

static const GUID CONDITIONS_ALE_CONNECT_REDIRECT[] =
{
	// Windows 7 and later:
	FWPM_CONDITION_ALE_APP_ID,
	FWPM_CONDITION_ALE_USER_ID,
	FWPM_CONDITION_FLAGS,
	FWPM_CONDITION_IP_LOCAL_ADDRESS,
	FWPM_CONDITION_IP_LOCAL_ADDRESS_TYPE,
	FWPM_CONDITION_IP_LOCAL_PORT,
	FWPM_CONDITION_IP_PROTOCOL,
	FWPM_CONDITION_IP_REMOTE_PORT,
	FWPM_CONDITION_IP_REMOTE_ADDRESS,
	FWPM_CONDITION_IP_DESTINATION_ADDRESS_TYPE,

	// Windows 8:
	//FWPM_CONDITION_ALE_PACKAGE_ID
};

static const GUID CONDITIONS_ALE_AUTH_CONNECT[] =
{
	FWPM_CONDITION_ALE_APP_ID,
	FWPM_CONDITION_ALE_REMOTE_MACHINE_ID,
	FWPM_CONDITION_ALE_REMOTE_USER_ID,
	FWPM_CONDITION_ALE_USER_ID,
	FWPM_CONDITION_FLAGS,
	FWPM_CONDITION_INTERFACE_TYPE,
	FWPM_CONDITION_IP_DESTINATION_ADDRESS_TYPE,
	FWPM_CONDITION_IP_LOCAL_ADDRESS,
	FWPM_CONDITION_IP_LOCAL_ADDRESS_TYPE,
	FWPM_CONDITION_IP_LOCAL_INTERFACE,
	FWPM_CONDITION_IP_LOCAL_PORT,
	FWPM_CONDITION_IP_PROTOCOL,
	FWPM_CONDITION_IP_REMOTE_ADDRESS,
	FWPM_CONDITION_IP_REMOTE_PORT,
	FWPM_CONDITION_SUB_INTERFACE_INDEX,
	FWPM_CONDITION_TUNNEL_TYPE,
	FWPM_CONDITION_IP_ARRIVAL_INTERFACE,
	FWPM_CONDITION_ARRIVAL_INTERFACE_TYPE,
	FWPM_CONDITION_ARRIVAL_TUNNEL_TYPE,
	FWPM_CONDITION_ARRIVAL_INTERFACE_INDEX,

	// Windows Vista with SP1 and later:
	FWPM_CONDITION_INTERFACE_INDEX,

	// Windows 7 and later:
	FWPM_CONDITION_NEXTHOP_SUB_INTERFACE_INDEX,
	FWPM_CONDITION_IP_NEXTHOP_INTERFACE,
	FWPM_CONDITION_NEXTHOP_INTERFACE_TYPE,
	FWPM_CONDITION_NEXTHOP_TUNNEL_TYPE,
	FWPM_CONDITION_NEXTHOP_INTERFACE_INDEX,
	FWPM_CONDITION_ORIGINAL_PROFILE_ID,
	FWPM_CONDITION_CURRENT_PROFILE_ID,
	FWPM_CONDITION_REAUTHORIZE_REASON,
	FWPM_CONDITION_PEER_NAME,
	FWPM_CONDITION_ORIGINAL_ICMP_TYPE,

	// Windows 8:
	//FWPM_CONDITION_ALE_PACKAGE_ID
};

static const GUID CONDITIONS_ALE_FLOW_ESTABLISHED[] =
{
	FWPM_CONDITION_ALE_APP_ID,
	FWPM_CONDITION_ALE_REMOTE_MACHINE_ID,
	FWPM_CONDITION_ALE_REMOTE_USER_ID,
	FWPM_CONDITION_ALE_USER_ID,
	FWPM_CONDITION_DIRECTION,
	FWPM_CONDITION_FLAGS,
	FWPM_CONDITION_INTERFACE_TYPE,
	FWPM_CONDITION_IP_DESTINATION_ADDRESS_TYPE,
	FWPM_CONDITION_IP_LOCAL_ADDRESS,
	FWPM_CONDITION_IP_LOCAL_ADDRESS_TYPE,
	FWPM_CONDITION_IP_LOCAL_INTERFACE,
	FWPM_CONDITION_IP_LOCAL_PORT,
	FWPM_CONDITION_IP_PROTOCOL,
	FWPM_CONDITION_IP_REMOTE_ADDRESS,
	FWPM_CONDITION_IP_REMOTE_PORT,
	FWPM_CONDITION_TUNNEL_TYPE,

	// Windows 8:
	//FWPM_CONDITION_ALE_PACKAGE_ID
};

static const GUID CONDITIONS_NAME_RESOLUTION_CACHE[] =
{
	// Windows 7 and later:
	FWPM_CONDITION_ALE_USER_ID,
	FWPM_CONDITION_ALE_APP_ID,
	FWPM_CONDITION_IP_REMOTE_ADDRESS,
	FWPM_CONDITION_PEER_NAME
};

static const GUID CONDITIONS_IPSEC_KM_DEMUX[] =
{
	FWPM_CONDITION_IP_LOCAL_ADDRESS,
	FWPM_CONDITION_IP_REMOTE_ADDRESS
};

static const GUID CONDITIONS_IPSEC[] =
{
	FWPM_CONDITION_IP_LOCAL_ADDRESS,
	FWPM_CONDITION_IP_LOCAL_PORT,
	FWPM_CONDITION_IP_PROTOCOL,
	FWPM_CONDITION_IP_REMOTE_ADDRESS,
	FWPM_CONDITION_IP_REMOTE_PORT,

	// Windows 7 and later:
	FWPM_CONDITION_IP_LOCAL_INTERFACE,
	FWPM_CONDITION_CURRENT_PROFILE_ID
};

static const GUID CONDITIONS_IKEEXT[] =
{
	FWPM_CONDITION_IP_LOCAL_ADDRESS,
	FWPM_CONDITION_IP_REMOTE_ADDRESS,

	// Windows 7 and later:
	FWPM_CONDITION_IP_LOCAL_INTERFACE,
	FWPM_CONDITION_CURRENT_PROFILE_ID
};

static const GUID CONDITIONS_RPC_UM[] =
{
	FWPM_CONDITION_DCOM_APP_ID,
	FWPM_CONDITION_IMAGE_NAME,
	FWPM_CONDITION_IP_LOCAL_ADDRESS_V4,
	FWPM_CONDITION_IP_LOCAL_ADDRESS_V6,
	FWPM_CONDITION_IP_LOCAL_PORT,
	FWPM_CONDITION_IP_REMOTE_ADDRESS_V4,
	FWPM_CONDITION_IP_REMOTE_ADDRESS_V6,
	FWPM_CONDITION_PIPE,
	FWPM_CONDITION_REMOTE_USER_TOKEN,
	FWPM_CONDITION_RPC_AUTH_LEVEL,
	FWPM_CONDITION_RPC_AUTH_TYPE,
	FWPM_CONDITION_RPC_IF_FLAG,
	FWPM_CONDITION_RPC_IF_UUID,
	FWPM_CONDITION_RPC_IF_VERSION,
	FWPM_CONDITION_RPC_PROTOCOL,
	FWPM_CONDITION_SEC_ENCRYPT_ALGORITHM,
	FWPM_CONDITION_SEC_KEY_SIZE
};

static const GUID CONDITIONS_RPC_EPMAP[] =
{
	FWPM_CONDITION_IP_LOCAL_ADDRESS_V4,
	FWPM_CONDITION_IP_LOCAL_ADDRESS_V6,
	FWPM_CONDITION_IP_LOCAL_PORT,
	FWPM_CONDITION_IP_REMOTE_ADDRESS_V4,
	FWPM_CONDITION_IP_REMOTE_ADDRESS_V6,
	FWPM_CONDITION_PIPE,
	FWPM_CONDITION_REMOTE_USER_TOKEN,
	FWPM_CONDITION_RPC_AUTH_LEVEL,
	FWPM_CONDITION_RPC_AUTH_TYPE,
	FWPM_CONDITION_RPC_IF_UUID,
	FWPM_CONDITION_RPC_IF_VERSION,
	FWPM_CONDITION_RPC_PROTOCOL,
	FWPM_CONDITION_SEC_ENCRYPT_ALGORITHM,
	FWPM_CONDITION_SEC_KEY_SIZE
};

static const GUID CONDITIONS_RPC_EP_ADD[] =
{
	FWPM_CONDITION_PROCESS_WITH_RPC_IF_UUID,
	FWPM_CONDITION_RPC_EP_FLAGS,
	FWPM_CONDITION_RPC_EP_VALUE,
	FWPM_CONDITION_RPC_PROTOCOL
};

static const GUID CONDITIONS_RPC_PROXY_CONN[] =
{
	FWPM_CONDITION_CLIENT_CERT_KEY_LENGTH,
	FWPM_CONDITION_CLIENT_CERT_OID,
	FWPM_CONDITION_CLIENT_TOKEN,
	FWPM_CONDITION_RPC_PROXY_AUTH_TYPE,
	FWPM_CONDITION_RPC_SERVER_NAME,
	FWPM_CONDITION_RPC_SERVER_PORT
};

static const GUID CONDITIONS_RPC_PROXY_IF[] =
{
	FWPM_CONDITION_CLIENT_CERT_KEY_LENGTH,
	FWPM_CONDITION_CLIENT_CERT_OID,
	FWPM_CONDITION_CLIENT_TOKEN,
	FWPM_CONDITION_RPC_IF_UUID,
	FWPM_CONDITION_RPC_IF_VERSION,
	FWPM_CONDITION_RPC_PROXY_AUTH_TYPE,
	FWPM_CONDITION_RPC_SERVER_NAME,
	FWPM_CONDITION_RPC_SERVER_PORT
};

static const GUID CONDITIONS_KM_AUTHORIZATION[] =
{
	// Windows 7 and later:
	FWPM_CONDITION_REMOTE_ID,
	FWPM_CONDITION_AUTHENTICATION_TYPE,
	FWPM_CONDITION_KM_TYPE,
	FWPM_CONDITION_KM_MODE,
	FWPM_CONDITION_DIRECTION,
	FWPM_CONDITION_IPSEC_POLICY_KEY
};

//static const GUID CONDITIONS_MAC_FRAME_ETHERNET[] =
//{
//	// Windows 8:
//	FWPM_CONDITION_INTERFACE_MAC_ADDRESS,
//	FWPM_CONDITION_MAC_LOCAL_ADDRESS,
//	FWPM_CONDITION_MAC_REMOTE_ADDRESS,
//	FWPM_CONDITION_MAC_LOCAL_ADDRESS_TYPE,
//	FWPM_CONDITION_MAC_REMOTE_ADDRESS_TYPE,
//	FWPM_CONDITION_ETHER_TYPE,
//	FWPM_CONDITION_VLAN_ID,
//	FWPM_CONDITION_INTERFACE,
//	FWPM_CONDITION_INTERFACE_INDEX,
//	FWPM_CONDITION_NDIS_PORT,
//	FWPM_CONDITION_L2_FLAGS
//};

//static const GUID CONDITIONS_MAC_FRAME_NATIVE[] =
//{
//	// Windows 8:
//	FWPM_CONDITION_NDIS_MEDIA_TYPE,
//	FWPM_CONDITION_NDIS_PHYSICAL_MEDIA_TYPE,
//	FWPM_CONDITION_INTERFACE,
//	FWPM_CONDITION_INTERFACE_TYPE,
//	FWPM_CONDITION_INTERFACE_INDEX,
//	FWPM_CONDITION_NDIS_PORT,
//	FWPM_CONDITION_L2_FLAGS
//};

//static const GUID CONDITIONS_VSWITCH_ETHERNET[] =
//{
//	// Windows 8:
//	FWPM_CONDITION_MAC_SOURCE_ADDRESS,
//	FWPM_CONDITION_MAC_SOURCE_ADDRESS_TYPE,
//	FWPM_CONDITION_MAC_DESTINATION_ADDRESS,
//	FWPM_CONDITION_MAC_DESTINATION_ADDRESS_TYPE,
//	FWPM_CONDITION_ETHER_TYPE,
//	FWPM_CONDITION_VLAN_ID,
//	FWPM_CONDITION_VSWITCH_TENANT_NETWORK_ID,
//	FWPM_CONDITION_VSWITCH_ID,
//	FWPM_CONDITION_VSWITCH_NETWORK_TYPE,
//	FWPM_CONDITION_VSWITCH_SOURCE_INTERFACE_ID,
//	FWPM_CONDITION_VSWITCH_SOURCE_INTERFACE_TYPE,
//	FWPM_CONDITION_VSWITCH_SOURCE_VM_ID,
//	FWPM_CONDITION_VSWITCH_L2_FLAGS
//};
//
//static const GUID CONDITIONS_VSWITCH_TRANSPORT[] =
//{
//	// Windows 8:
//	FWPM_CONDITION_IP_SOURCE_ADDRESS,
//	FWPM_CONDITION_IP_DESTINATION_ADDRESS,
//	FWPM_CONDITION_IP_PROTOCOL,
//	FWPM_CONDITION_IP_SOURCE_PORT,
//	FWPM_CONDITION_IP_DESTINATION_PORT,
//	FWPM_CONDITION_VLAN_ID,
//	FWPM_CONDITION_VSWITCH_TENANT_NETWORK_ID,
//	FWPM_CONDITION_VSWITCH_ID,
//	FWPM_CONDITION_VSWITCH_NETWORK_TYPE,
//	FWPM_CONDITION_VSWITCH_SOURCE_INTERFACE_ID,
//	FWPM_CONDITION_VSWITCH_SOURCE_INTERFACE_TYPE,
//	FWPM_CONDITION_VSWITCH_SOURCE_VM_ID,
//	FWPM_CONDITION_VSWITCH_DESTINATION_INTERFACE_ID,
//	FWPM_CONDITION_VSWITCH_DESTINATION_INTERFACE_TYPE,
//	FWPM_CONDITION_VSWITCH_L2_FLAGS
//};

struct ConditionsCollection
{
	const GUID *conditions;
	size_t numConditions;
};

#define MAKE_CONDITIONS_COLLECTION(x) ConditionsCollection { x, _countof(x) }

struct GuidLess
{
	bool operator()(const GUID a, const GUID b) const
	{
		return memcmp(&a, &b, sizeof(GUID)) < 0;
	}
};

typedef std::map<GUID, ConditionsCollection, GuidLess> ConditionMap;

ConditionMap g_lookup;
std::mutex g_lookupMutex;

void InitializeConditionMap(ConditionMap &lookup)
{
	ConditionMap temp
	{
		std::make_pair(FWPM_LAYER_INBOUND_IPPACKET_V4, MAKE_CONDITIONS_COLLECTION(CONDITIONS_INBOUND_IP_PACKET)),
		std::make_pair(FWPM_LAYER_INBOUND_IPPACKET_V4_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_INBOUND_IP_PACKET)),
		std::make_pair(FWPM_LAYER_INBOUND_IPPACKET_V6, MAKE_CONDITIONS_COLLECTION(CONDITIONS_INBOUND_IP_PACKET)),
		std::make_pair(FWPM_LAYER_INBOUND_IPPACKET_V6_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_INBOUND_IP_PACKET)),

		std::make_pair(FWPM_LAYER_OUTBOUND_IPPACKET_V4, MAKE_CONDITIONS_COLLECTION(CONDITIONS_OUTBOUND_IP_PACKET)),
		std::make_pair(FWPM_LAYER_OUTBOUND_IPPACKET_V4_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_OUTBOUND_IP_PACKET)),
		std::make_pair(FWPM_LAYER_OUTBOUND_IPPACKET_V6, MAKE_CONDITIONS_COLLECTION(CONDITIONS_OUTBOUND_IP_PACKET)),
		std::make_pair(FWPM_LAYER_OUTBOUND_IPPACKET_V6_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_OUTBOUND_IP_PACKET)),

		std::make_pair(FWPM_LAYER_IPFORWARD_V4, MAKE_CONDITIONS_COLLECTION(CONDITIONS_IPFORWARD)),
		std::make_pair(FWPM_LAYER_IPFORWARD_V4_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_IPFORWARD)),
		std::make_pair(FWPM_LAYER_IPFORWARD_V6, MAKE_CONDITIONS_COLLECTION(CONDITIONS_IPFORWARD)),
		std::make_pair(FWPM_LAYER_IPFORWARD_V6_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_IPFORWARD)),

		std::make_pair(FWPM_LAYER_INBOUND_TRANSPORT_V4, MAKE_CONDITIONS_COLLECTION(CONDITIONS_INBOUND_TRANSPORT)),
		std::make_pair(FWPM_LAYER_INBOUND_TRANSPORT_V4_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_INBOUND_TRANSPORT)),
		std::make_pair(FWPM_LAYER_INBOUND_TRANSPORT_V6, MAKE_CONDITIONS_COLLECTION(CONDITIONS_INBOUND_TRANSPORT)),
		std::make_pair(FWPM_LAYER_INBOUND_TRANSPORT_V6_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_INBOUND_TRANSPORT)),

		std::make_pair(FWPM_LAYER_OUTBOUND_TRANSPORT_V4, MAKE_CONDITIONS_COLLECTION(CONDITIONS_OUTBOUND_TRANSPORT)),
		std::make_pair(FWPM_LAYER_OUTBOUND_TRANSPORT_V4_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_OUTBOUND_TRANSPORT)),
		std::make_pair(FWPM_LAYER_OUTBOUND_TRANSPORT_V6, MAKE_CONDITIONS_COLLECTION(CONDITIONS_OUTBOUND_TRANSPORT)),
		std::make_pair(FWPM_LAYER_OUTBOUND_TRANSPORT_V6_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_OUTBOUND_TRANSPORT)),

		std::make_pair(FWPM_LAYER_STREAM_V4, MAKE_CONDITIONS_COLLECTION(CONDITIONS_STREAM)),
		std::make_pair(FWPM_LAYER_STREAM_V4_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_STREAM)),
		std::make_pair(FWPM_LAYER_STREAM_V6, MAKE_CONDITIONS_COLLECTION(CONDITIONS_STREAM)),
		std::make_pair(FWPM_LAYER_STREAM_V6_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_STREAM)),

		std::make_pair(FWPM_LAYER_DATAGRAM_DATA_V4, MAKE_CONDITIONS_COLLECTION(CONDITIONS_DATAGRAM_DATA)),
		std::make_pair(FWPM_LAYER_DATAGRAM_DATA_V4_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_DATAGRAM_DATA)),
		std::make_pair(FWPM_LAYER_DATAGRAM_DATA_V6, MAKE_CONDITIONS_COLLECTION(CONDITIONS_DATAGRAM_DATA)),
		std::make_pair(FWPM_LAYER_DATAGRAM_DATA_V6_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_DATAGRAM_DATA)),

		std::make_pair(FWPM_LAYER_STREAM_PACKET_V4, MAKE_CONDITIONS_COLLECTION(CONDITIONS_STREAM_PACKET)),
		std::make_pair(FWPM_LAYER_STREAM_PACKET_V6, MAKE_CONDITIONS_COLLECTION(CONDITIONS_STREAM_PACKET)),

		std::make_pair(FWPM_LAYER_INBOUND_ICMP_ERROR_V4, MAKE_CONDITIONS_COLLECTION(CONDITIONS_INBOUND_ICMP_ERROR)),
		std::make_pair(FWPM_LAYER_INBOUND_ICMP_ERROR_V4_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_INBOUND_ICMP_ERROR)),
		std::make_pair(FWPM_LAYER_INBOUND_ICMP_ERROR_V6, MAKE_CONDITIONS_COLLECTION(CONDITIONS_INBOUND_ICMP_ERROR)),
		std::make_pair(FWPM_LAYER_INBOUND_ICMP_ERROR_V6_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_INBOUND_ICMP_ERROR)),

		std::make_pair(FWPM_LAYER_OUTBOUND_ICMP_ERROR_V4, MAKE_CONDITIONS_COLLECTION(CONDITIONS_OUTBOUND_ICMP_ERROR)),
		std::make_pair(FWPM_LAYER_OUTBOUND_ICMP_ERROR_V4_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_OUTBOUND_ICMP_ERROR)),
		std::make_pair(FWPM_LAYER_OUTBOUND_ICMP_ERROR_V6, MAKE_CONDITIONS_COLLECTION(CONDITIONS_OUTBOUND_ICMP_ERROR)),
		std::make_pair(FWPM_LAYER_OUTBOUND_ICMP_ERROR_V6_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_OUTBOUND_ICMP_ERROR)),

		std::make_pair(FWPM_LAYER_ALE_BIND_REDIRECT_V4, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_BIND_REDIRECT)),
		std::make_pair(FWPM_LAYER_ALE_BIND_REDIRECT_V6, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_BIND_REDIRECT)),

		std::make_pair(FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V4, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_RESOURCE_ASSIGNMENT)),
		std::make_pair(FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V4_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_RESOURCE_ASSIGNMENT)),
		std::make_pair(FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V6, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_RESOURCE_ASSIGNMENT)),
		std::make_pair(FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V6_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_RESOURCE_ASSIGNMENT)),

		std::make_pair(FWPM_LAYER_ALE_RESOURCE_RELEASE_V4, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_RESOURCE_RELEASE)),
		std::make_pair(FWPM_LAYER_ALE_RESOURCE_RELEASE_V6, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_RESOURCE_RELEASE)),

		std::make_pair(FWPM_LAYER_ALE_ENDPOINT_CLOSURE_V4, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_ENDPOINT_CLOSURE)),
		std::make_pair(FWPM_LAYER_ALE_ENDPOINT_CLOSURE_V6, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_ENDPOINT_CLOSURE)),

		std::make_pair(FWPM_LAYER_ALE_AUTH_LISTEN_V4, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_AUTH_LISTEN)),
		std::make_pair(FWPM_LAYER_ALE_AUTH_LISTEN_V4_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_AUTH_LISTEN)),
		std::make_pair(FWPM_LAYER_ALE_AUTH_LISTEN_V6, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_AUTH_LISTEN)),
		std::make_pair(FWPM_LAYER_ALE_AUTH_LISTEN_V6_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_AUTH_LISTEN)),

		std::make_pair(FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_AUTH_RECV_ACCEPT)),
		std::make_pair(FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_AUTH_RECV_ACCEPT)),
		std::make_pair(FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_AUTH_RECV_ACCEPT)),
		std::make_pair(FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_AUTH_RECV_ACCEPT)),

		std::make_pair(FWPM_LAYER_ALE_CONNECT_REDIRECT_V4, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_CONNECT_REDIRECT)),
		std::make_pair(FWPM_LAYER_ALE_CONNECT_REDIRECT_V6, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_CONNECT_REDIRECT)),

		std::make_pair(FWPM_LAYER_ALE_AUTH_CONNECT_V4, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_AUTH_CONNECT)),
		std::make_pair(FWPM_LAYER_ALE_AUTH_CONNECT_V4_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_AUTH_CONNECT)),
		std::make_pair(FWPM_LAYER_ALE_AUTH_CONNECT_V6, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_AUTH_CONNECT)),
		std::make_pair(FWPM_LAYER_ALE_AUTH_CONNECT_V6_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_AUTH_CONNECT)),

		std::make_pair(FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_FLOW_ESTABLISHED)),
		std::make_pair(FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_FLOW_ESTABLISHED)),
		std::make_pair(FWPM_LAYER_ALE_FLOW_ESTABLISHED_V6, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_FLOW_ESTABLISHED)),
		std::make_pair(FWPM_LAYER_ALE_FLOW_ESTABLISHED_V6_DISCARD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_ALE_FLOW_ESTABLISHED)),

		std::make_pair(FWPM_LAYER_NAME_RESOLUTION_CACHE_V4, MAKE_CONDITIONS_COLLECTION(CONDITIONS_NAME_RESOLUTION_CACHE)),
		std::make_pair(FWPM_LAYER_NAME_RESOLUTION_CACHE_V6, MAKE_CONDITIONS_COLLECTION(CONDITIONS_NAME_RESOLUTION_CACHE)),

		std::make_pair(FWPM_LAYER_IPSEC_KM_DEMUX_V4, MAKE_CONDITIONS_COLLECTION(CONDITIONS_IPSEC_KM_DEMUX)),
		std::make_pair(FWPM_LAYER_IPSEC_KM_DEMUX_V6, MAKE_CONDITIONS_COLLECTION(CONDITIONS_IPSEC_KM_DEMUX)),

		std::make_pair(FWPM_LAYER_IPSEC_V4, MAKE_CONDITIONS_COLLECTION(CONDITIONS_IPSEC)),
		std::make_pair(FWPM_LAYER_IPSEC_V6, MAKE_CONDITIONS_COLLECTION(CONDITIONS_IPSEC)),

		std::make_pair(FWPM_LAYER_IKEEXT_V4, MAKE_CONDITIONS_COLLECTION(CONDITIONS_IKEEXT)),
		std::make_pair(FWPM_LAYER_IKEEXT_V6, MAKE_CONDITIONS_COLLECTION(CONDITIONS_IKEEXT)),

		// Single-occurrence layers
		std::make_pair(FWPM_LAYER_RPC_UM, MAKE_CONDITIONS_COLLECTION(CONDITIONS_RPC_UM)),
		std::make_pair(FWPM_LAYER_RPC_EPMAP, MAKE_CONDITIONS_COLLECTION(CONDITIONS_RPC_EPMAP)),
		std::make_pair(FWPM_LAYER_RPC_EP_ADD, MAKE_CONDITIONS_COLLECTION(CONDITIONS_RPC_EP_ADD)),
		std::make_pair(FWPM_LAYER_RPC_PROXY_CONN, MAKE_CONDITIONS_COLLECTION(CONDITIONS_RPC_PROXY_CONN)),
		std::make_pair(FWPM_LAYER_RPC_PROXY_IF, MAKE_CONDITIONS_COLLECTION(CONDITIONS_RPC_PROXY_IF)),
		std::make_pair(FWPM_LAYER_KM_AUTHORIZATION, MAKE_CONDITIONS_COLLECTION(CONDITIONS_KM_AUTHORIZATION)),

		//std::make_pair(FWPM_LAYER_INBOUND_MAC_FRAME_ETHERNET, MAKE_CONDITIONS_COLLECTION(CONDITIONS_MAC_FRAME_ETHERNET)),
		//std::make_pair(FWPM_LAYER_OUTBOUND_MAC_FRAME_ETHERNET, MAKE_CONDITIONS_COLLECTION(CONDITIONS_MAC_FRAME_ETHERNET)),

		//std::make_pair(FWPM_LAYER_INBOUND_MAC_FRAME_NATIVE, MAKE_CONDITIONS_COLLECTION(CONDITIONS_MAC_FRAME_NATIVE)),
		//std::make_pair(FWPM_LAYER_OUTBOUND_MAC_FRAME_NATIVE, MAKE_CONDITIONS_COLLECTION(CONDITIONS_MAC_FRAME_NATIVE)),

		//std::make_pair(FWPM_LAYER_EGRESS_VSWITCH_ETHERNET, MAKE_CONDITIONS_COLLECTION(CONDITIONS_VSWITCH_ETHERNET)),
		//std::make_pair(FWPM_LAYER_INGRESS_VSWITCH_ETHERNET, MAKE_CONDITIONS_COLLECTION(CONDITIONS_VSWITCH_ETHERNET)),

		//std::make_pair(FWPM_LAYER_EGRESS_VSWITCH_TRANSPORT_V4, MAKE_CONDITIONS_COLLECTION(CONDITIONS_VSWITCH_TRANSPORT)),
		//std::make_pair(FWPM_LAYER_INGRESS_VSWITCH_TRANSPORT_V4, MAKE_CONDITIONS_COLLECTION(CONDITIONS_VSWITCH_TRANSPORT)),
		//std::make_pair(FWPM_LAYER_EGRESS_VSWITCH_TRANSPORT_V6, MAKE_CONDITIONS_COLLECTION(CONDITIONS_VSWITCH_TRANSPORT)),
		//std::make_pair(FWPM_LAYER_INGRESS_VSWITCH_TRANSPORT_V4, MAKE_CONDITIONS_COLLECTION(CONDITIONS_VSWITCH_TRANSPORT)),
	};

	lookup = temp;
}

bool ExecuteConditionMap(std::function<bool(const ConditionMap &)> callback)
{
	std::lock_guard<std::mutex> lock(g_lookupMutex);

	if (g_lookup.empty())
	{
		InitializeConditionMap(g_lookup);
	}

	return callback(g_lookup);
}

ConditionsCollection GetLayerConditions(const GUID &layer)
{
	ConditionsCollection result;

	ExecuteConditionMap([&result, layer](const ConditionMap &lookup)
	{
		auto match = lookup.find(layer);

		if (lookup.end() == match)
		{
			throw std::runtime_error("Invalid layer GUID");
		}

		result = match->second;
		return true;
	});

	return result;
}

} // anon namespace

//static
bool LayerConditions::IsCompatible(const GUID &layer, const GUID &condition)
{
	auto layerConditions = GetLayerConditions(layer);

	for (size_t i = 0; i < layerConditions.numConditions; ++i)
	{
		if (0 == memcmp(&condition, &layerConditions.conditions[i], sizeof(GUID)))
		{
			return true;
		}
	}

	return false;
}

}
