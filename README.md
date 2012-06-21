What the hell is it?
====================

__Entry point for cocaine app engine.__ Technically speaking, it's an open-source balancer that enables you to communicate with cocaine cloud in simple and most importantly persistant manner. It's a library implemented in C++ which can be used from your application to send data to cocaine cloud.

Notable features:

* Cocaine Dealer keeps track of cocaine cloud nodes and shedules tasks to alive nodes only.
* No tasks are ever lost, you are guaranteed to get either proper responce or error.
* More than that, you can specify timeframe during which your task must be processed, so you will always get response in that timeframe, making Cocaine App Engine a soft-realtime system.
* Even more, balancer optionally supports persistant message delivery, which means that your tasks are stored in local or distributed storage before being sent to the cloud. This guarantees that sheduled tasks are not lost even in case of machine restart or complete hardware failure.
* Different balancing policies are supported, by default task can be distributed among cloud nodes in round-robin manner.
* Dealer also supports "smart balancing", here we take each cocaine node performance as well as network routing into account, so that tasks are sent to the closest and least busy cloud node. 
* State of the cocaine nodes, state of apps on those nodes, even apps handles are discovered and processed on-the-fly, no restarts needed.
* There are several bindings which allow to use balancer from different languages. In case your language is not supported, you can always implement your own binding. C++ API of Cocaine Dealer is dead-simple, so this won't be a problem at all.

At the moment, Cocaine Dealer supports the following languages and specifications:

* C++
* Python
* Perl

A motivating example
====================

Let's say you have a single cocaine node with a single app deployed. Let's say machine's ip is 192.168.0.2 and there is an app with name "image_processor" which has one handle "resize". 

First we need to create the file with the list of hosts where app is deployed.

image_processor.hosts file:
```txt
192.168.0.2
```

Then we need to write dealer configuration file, it's in plain old JSON:

dealer_config.json file:
```json
{
	"version" : 1,

	"services" :
	{
    	"image_processor_service" : {
			"app" : "image_processor",
			"autodiscovery" : {
				"type" : "FILE",
				"source" : "/path/to/image_processor.hosts"
			}
		}
	}
}
```

Let's see what we have here:
- "version", it's a version of the dealer configuration file.
- "services" object is where you describe each of your apps.
- "image_processor_service" it's an alias to your app, you use this alias to send tasks to app as described below.
- "image_processor" — actual name of the app in the cloud.
- "autodiscovery" section. That section tells dealer where to get the list of the cocaine nodes with deployed app.
- "type" — type of the source, in our case we specify that list of the hosts is stored locally in some file.
- "source" — path to image_processor.hosts

Ok, then what? Write some code!

```c++
#include <iostream>
#include <string>

#include "cocaine/dealer/dealer.hpp"

using namespace cocaine::dealer;

int main(int argc, char** argv) {

	// initialise balancer
	std::string config_path = "path/to/your/dealer_config.json";
	dealer_t dealer(config_path);

	// to which app/handle do we send our task
	message_path_t path("image_processor_service", "resize");

	// let's say you had your task data initialised somewhere previously
	void* task;
	size_t task_size;

	// send data
	boost::shared_ptr<response_t> response = dealer->send_message(task, task_size, path);

	// get response
	data_container response_data;

	try {
		response->get(&response_data);
	}
	catch (const dealer_error& err) {
		std::cout << "error code: " << err.code() << ", error message: " << err.what() << std::endl;
	}
	catch (const std::exception& ex) {
		std::cout << "error message: " << ex.what() << std::endl;
	}
	catch (...) {
		std::cout << "caught exception, no error message." << std::endl;
	}

	// process response_data
	// ...

	return EXIT_SUCCESS;
}
```

Simple as that!
