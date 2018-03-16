/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "postgres.h"
#include "access/htup.h"
#include "catalog/pg_type.h"
#include "catalog/pg_operator.h"
#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbllize.h"
#include "parser/parse_oper.h"
#include "utils/syscache.h"
#include "vcheck.h"


static const vFuncMap funcMap[] = {
		{INT2OID, &buildvint2, &destoryvint2, &vint2Size,&vint2serialization,&vint2deserialization},
		{INT4OID, &buildvint4, &destoryvint4, &vint4Size,&vint4serialization,&vint4deserialization},
		{INT8OID, &buildvint8, &destoryvint8, &vint8Size,&vint8serialization,&vint8deserialization},
		{FLOAT4OID, &buildvfloat4, &destoryvfloat4, &vfloat4Size,&vfloat4serialization,&vfloat4deserialization},
		{FLOAT8OID, &buildvfloat8, &destoryvfloat8, &vfloat8Size,&vfloat8serialization,&vfloat8deserialization},
		{BOOLOID, &buildvbool, &destoryvbool, &vboolSize,&vboolserialization,&vbooldeserialization}
};
static const int funcMapLen = sizeof(funcMap) /sizeof(vFuncMap);

static const vFuncMap* vFuncWalker(Oid type)
{
	for(int i = 0;i < funcMapLen; i ++)
		if(funcMap[i].ntype == type) return &funcMap[i];

	return NULL;
}

static HTAB *hashMapVFunc = NULL;

typedef struct VecFuncHashEntry
{
    Oid src;
    vFuncMap *vFunc;
} VecFuncHashEntry;


const vFuncMap* GetVFunc(Oid vtype){
	HeapTuple tuple;
	VecFuncHashEntry *entry = NULL;
	bool found = false;
	Form_pg_type pt;


	//construct the hash table
	if(NULL == hashMapVFunc)
	{
		HASHCTL	hash_ctl;
		MemSet(&hash_ctl, 0, sizeof(hash_ctl));

		hash_ctl.keysize = sizeof(Oid);
		hash_ctl.entrysize = sizeof(VecFuncHashEntry);
		hash_ctl.hash = oid_hash;

		hashMapVFunc = hash_create("hashvfunc", 64/*enough?*/, &hash_ctl, HASH_ELEM | HASH_FUNCTION);
	}

	//first, find the vectorized type in hash table
	entry = hash_search(hashMapVFunc, &vtype, HASH_ENTER, &found);
	if(found)
		return entry->vFunc;

	Assert(!found);

	tuple = SearchSysCache1(TYPEOID,
	                            ObjectIdGetDatum(vtype));


	if(!HeapTupleIsValid(tuple))
	{
		ReleaseSysCache(tuple);
		return InvalidOid;
	}

	pt = (Form_pg_type) GETSTRUCT(tuple);

	/* storage in hash table*/
	entry->vFunc = vFuncWalker(pt->typelem);

	ReleaseSysCache(tuple);

	return entry->vFunc ;
}
