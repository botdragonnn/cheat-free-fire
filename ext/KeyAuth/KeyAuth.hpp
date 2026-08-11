#pragma once
#include <string>
#include <vector>

namespace KeyAuth {

	struct Subscription {
		std::string name;
		std::string expiry;
	};

	class api {
	public:
		std::string name;
		std::string ownerid;
		std::string secret;
		std::string version;
		std::string url;

		bool initalized = false;
		bool success = false;
		std::string message;
		std::string username;
		std::vector<Subscription> subscriptions;

		api(const std::string& appName, const std::string& owner, const std::string& sec, const std::string& ver, const std::string& apiUrl);

		bool init();
		bool license(const std::string& key);

		std::string getExpiry() const;

	private:
		std::string sessionid;
		std::string enckey;
	};

}
