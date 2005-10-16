/*
 * Copyright (c) 2004, 2005 TADA AB - Taby Sweden
 * Distributed under the terms shown in the file COPYRIGHT
 * found in the root folder of this project or at
 * http://eng.tada.se/osprojects/COPYRIGHT.html
 *
 * @author Thomas Hallgren
 */
#include "org_postgresql_pljava_internal_SPI.h"
#include "pljava/backports.h"
#include "pljava/SPI.h"
#include "pljava/Backend.h"
#include "pljava/Exception.h"
#include "pljava/type/String.h"
#include "pljava/type/TupleTable.h"

Datum SPI_initialize(PG_FUNCTION_ARGS)
{
	JNIEnv* env = (JNIEnv*)PG_GETARG_POINTER(0);

	JNINativeMethod methods[] = {
		{
		"_exec",
	  	"(JLjava/lang/String;I)I",
	  	Java_org_postgresql_pljava_internal_SPI__1exec
		},
		{
		"_getProcessed",
		"()I",
		Java_org_postgresql_pljava_internal_SPI__1getProcessed
		},
		{
		"_getResult",
		"()I",
		Java_org_postgresql_pljava_internal_SPI__1getResult
		},
		{
		"_getTupTable",
		"()Lorg/postgresql/pljava/internal/TupleTable;",
		Java_org_postgresql_pljava_internal_SPI__1getTupTable
		},
		{ 0, 0, 0 }};

	PgObject_registerNatives(env, "org/postgresql/pljava/internal/SPI", methods);

	PG_RETURN_VOID();
}

/****************************************
 * JNI methods
 ****************************************/
/*
 * Class:     org_postgresql_pljava_internal_SPI
 * Method:    _exec
 * Signature: (JLjava/lang/String;I)I
 */
JNIEXPORT jint JNICALL
Java_org_postgresql_pljava_internal_SPI__1exec(JNIEnv* env, jclass cls, jlong threadId, jstring cmd, jint count)
{
	STACK_BASE_VARS
	char* command;
	jint result;
	PLJAVA_ENTRY_FENCE(0)

	command = String_createNTS(env, cmd);
	if(command == 0)
		return 0;

	result = 0;
	STACK_BASE_PUSH(threadId)
	Backend_pushJavaFrame(env);
	PG_TRY();
	{
		Backend_assertConnect();
		result = (jint)SPI_exec(command, (int)count);
		if(result < 0)
			Exception_throwSPI(env, "exec", result);

		Backend_popJavaFrame(env);
		pfree(command);
		STACK_BASE_POP()
	}
	PG_CATCH();
	{
		Backend_popJavaFrame(env);
		STACK_BASE_POP()
		Exception_throw_ERROR(env, "SPI_exec");
	}
	PG_END_TRY();
	return result;
}

/*
 * Class:     org_postgresql_pljava_internal_SPI
 * Method:    _getProcessed
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_org_postgresql_pljava_internal_SPI__1getProcessed(JNIEnv* env, jclass cls)
{
	return (jint)SPI_processed;
}

/*
 * Class:     org_postgresql_pljava_internal_SPI
 * Method:    _getResult
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_org_postgresql_pljava_internal_SPI__1getResult(JNIEnv* env, jclass cls)
{
	return (jint)SPI_result;
}

/*
 * Class:     org_postgresql_pljava_internal_SPI
 * Method:    _getTupTable
 * Signature: ()Lorg/postgresql/pljava/internal/TupleTable;
 */
JNIEXPORT jobject JNICALL
Java_org_postgresql_pljava_internal_SPI__1getTupTable(JNIEnv* env, jclass cls)
{
	jobject tupleTable = 0;
	SPITupleTable* tts = SPI_tuptable;

	if(tts != 0)
	{
		PLJAVA_ENTRY_FENCE(0)
		tupleTable = TupleTable_create(env, tts);
		SPI_freetuptable(tts);
		SPI_tuptable = 0;
	}
	return tupleTable;
}

static void assertXid(SubTransactionId xid)
{
	if(xid != GetCurrentSubTransactionId())
	{
		/* Oops. Rollback to top level transaction.
		 */
		ereport(ERROR, (
			errcode(ERRCODE_INVALID_TRANSACTION_TERMINATION),
			errmsg("Subtransaction mismatch at txlevel %d",
				GetCurrentTransactionNestLevel())));
	}
}

Savepoint* SPI_setSavepoint(const char* name)
{
	/* We let the savepoint live in the current MemoryContext. It will be released
	 * or rolled back even if the creator forgets about it.
	 */
	Savepoint* sp = (Savepoint*)palloc(sizeof(Savepoint) + strlen(name));
	Backend_assertConnect();
	BeginInternalSubTransaction((char*)name);
	sp->nestingLevel = GetCurrentTransactionNestLevel();
	sp->xid = GetCurrentSubTransactionId();
	strcpy(sp->name, name);
	return sp;
}

void SPI_releaseSavepoint(Savepoint* sp)
{
	while(sp->nestingLevel < GetCurrentTransactionNestLevel())
		ReleaseCurrentSubTransaction();

	if(sp->nestingLevel == GetCurrentTransactionNestLevel())
	{
		assertXid(sp->xid);
		ReleaseCurrentSubTransaction();
	}
	pfree(sp);
}

void SPI_rollbackSavepoint(Savepoint* sp)
{
	while(sp->nestingLevel < GetCurrentTransactionNestLevel())
		RollbackAndReleaseCurrentSubTransaction();

	if(sp->nestingLevel == GetCurrentTransactionNestLevel())
	{
		assertXid(sp->xid);
		RollbackAndReleaseCurrentSubTransaction();
	}
	SPI_restore_connection();
	pfree(sp);
}

