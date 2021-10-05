import sys
import time
import importlib
from utils import color_text_green, color_text_blue, color_text_yellow

if len(sys.argv) < 2:
    print(color_text_green("[MONITOR] required arguments: config_file"))
    sys.exit()

config = None
config_file = sys.argv[1]

try:
    config = importlib.import_module(config_file)
except Exception as e:
    print(color_text_green("Exception: {}".format(e)))
    sys.exit(1)

print(color_text_yellow("finish imoporting {}".format(config_file)))

def retrieveApp(cmd_list):
    app_list = []
    if len(cmd_list) == 1:
        print(color_text_yellow("[START] {} all the apps".format(cmd_list[0])))
        if cmd_list[0] == 'commit':
            app_list = config.apps[:]
        else:
            app_list = config.scheduler.apps[:]
    else:
        # operate only on given apps
        app_indices = []
        for i in range(1, len(cmd_list)):
            app_idx = int(cmd_list[i])
            if app_idx >= len(config.apps):
                continue
            app_indices.append(app_idx)
            app_list.append(config.apps[app_idx])
        print(color_text_yellow("[START] {} apps: {}".format(cmd_list[0], app_indices)))
    return app_list

while True:
    try:
        cmd = input(color_text_blue("Enter command: "))
    except KeyboardInterrupt:
        print('')
        sys.exit()

    cmd_list = cmd.split(' ')

    # network operations followed by application operations
    if cmd_list[0] not in [ 'setup', 'change', 'udp', 'route', 'destroy',
                            'commit', 'check', 'reset', 'close' ]:
        print(color_text_green("Usage: {  setup | change | udp | route | destroy |"
                               " commit | check | reset | close }"))
        continue

    if cmd_list[0] == 'setup':
        config.scheduler.setup_network()

    elif cmd_list[0] == 'change':
        config.scheduler.change_network_bandwidth(config.new_bws)

    elif cmd_list[0] == 'udp':
        config.scheduler.add_udp_traffic()

    elif cmd_list[0] == 'route':
        config.scheduler.add_ip_route()

    elif cmd_list[0] == 'destroy':
        config.scheduler.destroy_network()

    elif cmd_list[0] == 'commit':
        app_list = retrieveApp(cmd_list)
        config.scheduler.commit_new_apps(app_list)

    elif cmd_list[0] == 'check':
        app_list = retrieveApp(cmd_list)
        for app in app_list:
            app.health_check()

    elif cmd_list[0] == 'reset':
        app_list = retrieveApp(cmd_list)
        for app in app_list:
            app.reset()

    elif cmd_list[0] == 'close':
        app_list = retrieveApp(cmd_list)
        config.scheduler.terminate_old_apps(app_list)


    print(color_text_yellow("[FINISH] {}".format(cmd_list[0])))
