# --- BEGIN COPYRIGHT BLOCK ---
# Copyright (C) 2016 Red Hat, Inc.
# All rights reserved.
#
# License: GPL (version 3 or any later version).
# See LICENSE for details.
# --- END COPYRIGHT BLOCK ---
#
import threading

import pytest
from lib389.tasks import *
from lib389.utils import *
from lib389.topologies import topology_m4

from lib389._constants import (DEFAULT_SUFFIX, REPLICA_RUV_FILTER, ReplicaRole,
                              REPLICAID_MASTER_4, REPLICAID_MASTER_3, REPLICAID_MASTER_2,
                              REPLICAID_MASTER_1, REPLICATION_BIND_DN, REPLICATION_BIND_PW,
                              REPLICATION_BIND_METHOD, REPLICATION_TRANSPORT, SUFFIX,
                              RA_NAME, RA_BINDDN, RA_BINDPW, RA_METHOD, RA_TRANSPORT_PROT,
                              defaultProperties, args_instance)

from lib389 import DirSrv

logging.getLogger(__name__).setLevel(logging.DEBUG)
log = logging.getLogger(__name__)


def remove_master4_agmts(msg, topology_m4):
    """Remove all the repl agmts to master4. """

    log.info('%s: remove all the agreements to master 4...' % msg)
    for num in range(1, 4):
        try:
            topology_m4.ms["master{}".format(num)].agreement.delete(DEFAULT_SUFFIX,
                                                                    topology_m4.ms["master4"].host,
                                                                    topology_m4.ms["master4"].port)
        except ldap.LDAPError as e:
            log.fatal('{}: Failed to delete agmt(m{} -> m4), error: {}'.format(msg, num, str(e)))
            assert False


def restore_master4(topology_m4, newReplicaId=REPLICAID_MASTER_4):
    """In our tests will always be removing master 4, so we need a common
    way to restore it for another test
    """

    log.info('Restoring master 4...')

    # Enable replication on master 4
    topology_m4.ms["master4"].replica.enableReplication(suffix=SUFFIX, role=ReplicaRole.MASTER,
                                                        replicaId=newReplicaId)

    for num in range(1, 4):
        host_to = topology_m4.ms["master{}".format(num)].host
        port_to = topology_m4.ms["master{}".format(num)].port
        properties = {RA_NAME: 'meTo_{}:{}'.format(host_to, port_to),
                      RA_BINDDN: defaultProperties[REPLICATION_BIND_DN],
                      RA_BINDPW: defaultProperties[REPLICATION_BIND_PW],
                      RA_METHOD: defaultProperties[REPLICATION_BIND_METHOD],
                      RA_TRANSPORT_PROT: defaultProperties[REPLICATION_TRANSPORT]}
        agmt = topology_m4.ms["master4"].agreement.create(suffix=SUFFIX, host=host_to,
                                                          port=port_to, properties=properties)
        if not agmt:
            log.fatal("Fail to create a master -> master replica agreement")
            assert False
        log.debug("%s created" % agmt)

        host_to = topology_m4.ms["master4"].host
        port_to = topology_m4.ms["master4"].port
        properties = {RA_NAME: 'meTo_{}:{}'.format(host_to, port_to),
                      RA_BINDDN: defaultProperties[REPLICATION_BIND_DN],
                      RA_BINDPW: defaultProperties[REPLICATION_BIND_PW],
                      RA_METHOD: defaultProperties[REPLICATION_BIND_METHOD],
                      RA_TRANSPORT_PROT: defaultProperties[REPLICATION_TRANSPORT]}
        agmt = topology_m4.ms["master{}".format(num)].agreement.create(suffix=SUFFIX, host=host_to,
                                                                       port=port_to, properties=properties)
        if not agmt:
            log.fatal("Fail to create a master -> master replica agreement")
            assert False
        log.debug("%s created" % agmt)

    # Stop the servers - this allows the rid(for master4) to be used again
    for num in range(1, 5):
        topology_m4.ms["master{}".format(num)].stop(timeout=30)

    # Initialize the agreements
    topology_m4.ms["master1"].start(timeout=30)
    for num in range(2, 5):
        host_to = topology_m4.ms["master{}".format(num)].host
        port_to = topology_m4.ms["master{}".format(num)].port
        topology_m4.ms["master{}".format(num)].start(timeout=30)
        time.sleep(5)
        topology_m4.ms["master1"].agreement.init(SUFFIX, host_to, port_to)
        agreement = topology_m4.ms["master1"].agreement.list(suffix=SUFFIX,
                                                             consumer_host=host_to,
                                                             consumer_port=port_to)[0].dn
        topology_m4.ms["master1"].waitForReplInit(agreement)

    time.sleep(5)

    # Test Replication is working
    for num in range(2, 5):
        if topology_m4.ms["master1"].testReplication(DEFAULT_SUFFIX, topology_m4.ms["master{}".format(num)]):
            log.info('Replication is working m1 -> m{}.'.format(num))
        else:
            log.fatal('restore_master4: Replication is not working from m1 -> m{}.'.format(num))
            assert False
        time.sleep(1)

    # Check replication is working from master 4 to master1...
    if topology_m4.ms["master4"].testReplication(DEFAULT_SUFFIX, topology_m4.ms["master1"]):
        log.info('Replication is working m4 -> m1.')
    else:
        log.fatal('restore_master4: Replication is not working from m4 -> 1.')
        assert False
    time.sleep(5)

    log.info('Master 4 has been successfully restored.')


def test_ticket49180(topology_m4):

    log.info('Running test_ticket49180...')

    log.info('Check that replication works properly on all masters')
    agmt_nums = {"master1": ("2", "3", "4"),
                 "master2": ("1", "3", "4"),
                 "master3": ("1", "2", "4"),
                 "master4": ("1", "2", "3")}

    for inst_name, agmts in agmt_nums.items():
        for num in agmts:
            if not topology_m4.ms[inst_name].testReplication(DEFAULT_SUFFIX, topology_m4.ms["master{}".format(num)]):
                log.fatal(
                    'test_replication: Replication is not working between {} and master {}.'.format(inst_name,
                                                                                                    num))
                assert False

    # Disable master 4
    log.info('test_clean: disable master 4...')
    topology_m4.ms["master4"].replica.disableReplication(DEFAULT_SUFFIX)

    # Remove the agreements from the other masters that point to master 4
    remove_master4_agmts("test_clean", topology_m4)

    # Cleanup - restore master 4
    restore_master4(topology_m4, 4444)


    attr_errors = os.popen('egrep "attrlist_replace" %s  | wc -l' % topology_m4.ms["master1"].errlog)
    ecount = int(attr_errors.readline().rstrip())  
    log.info("Errors found on m1: %d" % ecount)
    assert (ecount == 0)

    attr_errors = os.popen('egrep "attrlist_replace" %s  | wc -l' % topology_m4.ms["master2"].errlog)
    ecount = int(attr_errors.readline().rstrip())  
    log.info("Errors found on m2: %d" % ecount)
    assert (ecount == 0)

    attr_errors = os.popen('egrep "attrlist_replace" %s  | wc -l' % topology_m4.ms["master3"].errlog)
    ecount = int(attr_errors.readline().rstrip())  
    log.info("Errors found on m3: %d" % ecount)
    assert (ecount == 0)

if __name__ == '__main__':
    # Run isolated
    # -s for DEBUG mode
    CURRENT_FILE = os.path.realpath(__file__)
    pytest.main("-s %s" % CURRENT_FILE)
