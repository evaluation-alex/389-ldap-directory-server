# --- BEGIN COPYRIGHT BLOCK ---
# Copyright (C) 2015 Red Hat, Inc.
# All rights reserved.
#
# License: GPL (version 3 or any later version).
# See LICENSE for details.
# --- END COPYRIGHT BLOCK ---

import ldap

import sys
from lib389._constants import *
from lib389.properties import *
from lib389 import Entry
from lib389.utils import ensure_str, ensure_bytes

from lib389._mapped_object import DSLdapObjects, DSLdapObject

MAJOR, MINOR, _, _, _ = sys.version_info

if MAJOR >= 3 or (MAJOR == 2 and MINOR >= 7):
    from ldap.controls.readentry import PostReadControl

DEFAULT_INDEX_DN = "cn=default indexes,%s" % DN_CONFIG_LDBM


class Index(DSLdapObject):
    """Index DSLdapObject with:
    - must attributes = ['cn', 'nsSystemIndex', 'nsIndexType']
    - RDN attribute is 'cn'

    :param instance: An instance
    :type instance: lib389.DirSrv
    :param dn: Index DN
    :type dn: str
    """

    def __init__(self, instance, dn=None):
        super(Index, self).__init__(instance, dn)
        self._rdn_attribute = 'cn'
        self._must_attributes = ['cn', 'nsSystemIndex', 'nsIndexType']
        self._create_objectclasses = ['top', 'nsIndex']
        self._protected = False
        self._lint_functions = []


class Indexes(DSLdapObjects):
    """DSLdapObjects that represents Index

    :param instance: An instance
    :type instance: lib389.DirSrv
    :param basedn: DN of suffix container.
    :type basedn: str
    """

    def __init__(self, instance, basedn=DEFAULT_INDEX_DN):
        super(Indexes, self).__init__(instance=instance)
        self._objectclasses = ['nsIndex']
        self._filterattrs = ['cn']
        self._childobject = Index
        self._basedn = basedn


class IndexLegacy(object):

    def __init__(self, conn):
        """@param conn - a DirSrv instance"""
        self.conn = conn
        self.log = conn.log

    def delete_all(self, benamebase):
        benamebase = ensure_str(benamebase)
        dn = "cn=index,cn=" + benamebase + "," + DN_LDBM

        # delete each defined index
        self.conn.delete_branch_s(dn, ldap.SCOPE_ONELEVEL)

        # Then delete the top index entry
        self.log.debug("Delete head index entry %s" % (dn))
        self.conn.delete_s(dn)

    def create(self, suffix=None, be_name=None, attr=None, args=None):
        if not suffix and not be_name:
            raise ValueError("suffix/backend name is mandatory parameter")

        if not attr:
            raise ValueError("attr is mandatory parameter")

        indexTypes = args.get(INDEX_TYPE, None)
        matchingRules = args.get(INDEX_MATCHING_RULE, None)

        return self.addIndex(suffix, be_name, attr, indexTypes=indexTypes,
                             matchingRules=matchingRules)

    def addIndex(self, suffix, be_name, attr, indexTypes, matchingRules,
                 postReadCtrl=None):
        """Specify the suffix (should contain 1 local database backend),
            the name of the attribute to index, and the types of indexes
            to create e.g. "pres", "eq", "sub"
        """
        msg_id = None
        if be_name:
            dn = ('cn=%s,cn=index,cn=%s,cn=ldbm database,cn=plugins,cn=config'
                  % (attr, be_name))
        else:
            entries_backend = self.conn.backend.list(suffix=suffix)
            # assume 1 local backend
            dn = "cn=%s,cn=index,%s" % (attr, entries_backend[0].dn)

        if postReadCtrl:
            add_record = [('nsSystemIndex', ['false']),
                          ('cn', [attr]),
                          ('objectclass', ['top', 'nsindex']),
                          ('nsIndexType', indexTypes)]
            if matchingRules:
                add_record.append(('nsMatchingRule', matchingRules))

        else:
            entry = Entry(dn)
            entry.setValues('objectclass', 'top', 'nsIndex')
            entry.setValues('cn', attr)
            entry.setValues('nsSystemIndex', "false")
            entry.setValues('nsIndexType', indexTypes)
            if matchingRules:
                entry.setValues('nsMatchingRule', matchingRules)

        if MAJOR >= 3 or (MAJOR == 2 and MINOR >= 7):
            try:
                if postReadCtrl:
                    pr = PostReadControl(criticality=True, attrList=['*'])
                    msg_id = self.conn.add_ext(dn, add_record, serverctrls=[pr])
                else:
                    self.conn.add_s(entry)
            except ldap.LDAPError as e:
                raise e

        return msg_id

    def modIndex(self, suffix, attr, mod):
        """just a wrapper around a plain old ldap modify, but will
        find the correct index entry based on the suffix and attribute"""
        entries_backend = self.conn.backend.list(suffix=suffix)
        # assume 1 local backend
        dn = "cn=%s,cn=index,%s" % (attr, entries_backend[0].dn)
        self.conn.modify_s(dn, mod)
