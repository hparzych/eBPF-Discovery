// SPDX-License-Identifier: Apache-2.0

#include "service/Aggregator.h"

#include "ebpfdiscovery/IpAddressChecker.h"

#include <algorithm>
#include <gmock/gmock.h>

using namespace ebpfdiscovery;
using namespace service;

class IpAddressCheckerMock : public IpAddressCheckerInerface {
public:
	MOCK_METHOD(bool, isAddressExternalLocal, (IPv4int), (override));
};

struct ServiceAggregatorTest : public testing::Test {
	std::pair<httpparser::HttpRequest, DiscoverySessionMeta> makeRequest(
			int pid, std::string host, std::string url, std::optional<__u8> flags = std::nullopt) {
		httpparser::HttpRequest request;
		request.host = host;
		request.url = url;
		DiscoverySessionMeta meta;
		if (flags) {
			meta.flags |= *flags;
		}
		meta.pid = pid;
		return {request, meta};
	}

	IpAddressCheckerMock ipCheckerMock;
	Aggregator aggregator{ipCheckerMock};
};

TEST_F(ServiceAggregatorTest, aggregate) {
	EXPECT_EQ(aggregator.popServices().size(), 0);
	// Service 1
	{
		const auto [request, meta]{makeRequest(100, "host", "/url", DISCOVERY_SESSION_FLAGS_IPV4)};
		EXPECT_CALL(ipCheckerMock, isAddressExternalLocal).WillOnce(testing::Return(true));
		aggregator.newRequest(request, meta);
	}
	{
		auto [request, meta] = makeRequest(100, "host", "/url");
		aggregator.newRequest(request, meta);
	}
	// Service 2
	{
		auto [request, meta] = makeRequest(100, "host", "/url2", DISCOVERY_SESSION_FLAGS_IPV4);
		EXPECT_CALL(ipCheckerMock, isAddressExternalLocal).WillOnce(testing::Return(false));
		aggregator.newRequest(request, meta);
	}
	// Service 3
	{
		auto [request, meta] = makeRequest(200, "host", "/url2", DISCOVERY_SESSION_FLAGS_IPV4);
		EXPECT_CALL(ipCheckerMock, isAddressExternalLocal).WillOnce(testing::Return(true));
		aggregator.newRequest(request, meta);
	}
	{
		auto [request, meta] = makeRequest(200, "host", "/url2", DISCOVERY_SESSION_FLAGS_IPV4);
		EXPECT_CALL(ipCheckerMock, isAddressExternalLocal).WillOnce(testing::Return(false));
		aggregator.newRequest(request, meta);
	}
	{
		auto [request, meta] = makeRequest(200, "host", "/url2", DISCOVERY_SESSION_FLAGS_IPV4);
		EXPECT_CALL(ipCheckerMock, isAddressExternalLocal).WillOnce(testing::Return(true));
		aggregator.newRequest(request, meta);
	}

	{
		auto services = aggregator.popServices();
		EXPECT_EQ(services.size(), 3);

		Service expectedService1{.pid{100}, .endpoint{"host/url"}, .internalClientsNumber{0}, .externalClientsNumber{2}};
		Service expectedService2{.pid{100}, .endpoint{"host/url2"}, .internalClientsNumber{1}, .externalClientsNumber{0}};
		Service expectedService3{.pid{200}, .endpoint{"host/url2"}, .internalClientsNumber{1}, .externalClientsNumber{2}};
		EXPECT_THAT(services, testing::Contains(expectedService1));
		EXPECT_THAT(services, testing::Contains(expectedService2));
		EXPECT_THAT(services, testing::Contains(expectedService3));
	}
	EXPECT_EQ(aggregator.popServices().size(), 0);
}