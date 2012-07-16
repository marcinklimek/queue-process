#include <iostream>
#include <string>
#include <boost/thread.hpp>
#include <boost/unordered_map.hpp>

#define NDEBUG
#include "redisclient.h"

using namespace std;

class Configuration
{
    boost::unordered_map<string, string> _config;
    
public:

    Configuration()
    {
        _config["host"]  = "localhost";
        _config["queue"] = "iscdhcp::events::queue";
        _config["work"]  = "iscdhcp::events::queue_work";
    }
    
    string& operator[](std::string key)
    {
        return _config[key];
    };
};

static Configuration config;


class SafeRedisClient
{
    Configuration& _config;
    boost::shared_ptr<redis::client> _client;
    
    bool _connected;
    
public:
    
    SafeRedisClient(Configuration& config) : _connected(false), _config( config )
    {
        connect();
    }

    void connect()
    {
        do
        {
            try
            {
                _client = boost::shared_ptr<redis::client>( new redis::client( _config["host"] ) );

                redis::server_info data;
                _client->info(data);

                cout << "Connected to redis v: " << data.version << endl;
                
                _connected = true;
            }
            catch( redis::redis_error& e )
            {
                cout << "Exection "  << e.what() << endl;
                cout << "Sleeping " << endl;
                boost::this_thread::sleep( boost::posix_time::seconds(1) );
            }
        }
        while (!_connected);
    }

    string blpop(const string& key)
    {
        bool done = false;

        do
        {
            try
            {
                string val = _client->blpop( key, 1);
                done = true;

                return val;
            }
            catch (redis::connection_error& e)
            {
                cout << "Exception: " << e.what() << endl;

                _connected = false;
                connect();
            }
        }
        while( !done );

        return redis::client::missing_value();
    }
};

class Forwarder
{
    Configuration& _config;

    boost::thread_group _workers;
    
    boost::mutex _mutex;
    boost::condition_variable _condition;
    bool _stop;

    
public:
    
    Forwarder(Configuration& config) : _config(config)
    {
        _workers.create_thread( boost::bind( &Forwarder::process_queue, this ));
        _workers.create_thread( boost::bind( &Forwarder::monitor, this ));
    }

    ~Forwarder()
    {
        _workers.interrupt_all();
        _workers.join_all();
    }



    
    void process_queue()
    {
        try
        {
            SafeRedisClient client( _config );

            while( !_stop )
            {
                string value = client.blpop( _config["queue"] );

                boost::unique_lock<boost::mutex> lock(_mutex);

                if ( value == redis::client::missing_value() )
                    continue;

                cout << boost::this_thread::get_id() << ": " <<  value << endl;

                _condition.notify_one();
            }
        }
        catch(const boost::thread_interrupted& e)
        {
            cout << "process queue interrupted" << endl;
            _stop = true;
        }
    }

    void monitor()
    {
        try
        {
            boost::unique_lock<boost::mutex> lock(_mutex);

            while( !_stop )
            {
                _condition.wait( lock );
                cout << "event processed" << endl;
            }
        }
        catch(const boost::thread_interrupted& e)
        {
            cout << "process monitor interrupted" << endl;
            _stop = true;
        }
    }
};

int main(int argc, char **argv)
{
    cout << config["host"] << endl;

    Forwarder forwarder( config );

    //wait
    cin.get();
   
    return 0;
}
