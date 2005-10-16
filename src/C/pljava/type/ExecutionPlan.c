/*
 * Copyright (c) 2004, 2005 TADA AB - Taby Sweden
 * Distributed under the terms shown in the file COPYRIGHT
 * found in the root folder of this project or at
 * http://eng.tada.se/osprojects/COPYRIGHT.html
 *
 * @author Thomas Hallgren
 */
#include <postgres.h>
#include <executor/tuptable.h>

#include "org_postgresql_pljava_internal_ExecutionPlan.h"
#include "pljava/Backend.h"
#include "pljava/Exception.h"
#include "pljava/Function.h"
#include "pljava/MemoryContext.h"
#include "pljava/SPI.h"
#include "pljava/type/Type_priv.h"
#include "pljava/type/Oid.h"
#include "pljava/type/Portal.h"
#include "pljava/type/String.h"
#include "pljava/type/ExecutionPlan.h"

#include <utils/guc.h>

static Type      s_ExecutionPlan;
static TypeClass s_ExecutionPlanClass;
static jclass    s_ExecutionPlan_class;

static Type ExecutionPlan_obtain(Oid typeId)
{
	return s_ExecutionPlan;
}

/* Make this datatype available to the postgres system.
 */
extern Datum ExecutionPlan_initialize(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(ExecutionPlan_initialize);
Datum ExecutionPlan_initialize(PG_FUNCTION_ARGS)
{
	JNINativeMethod methods[] = {
		{
		"_cursorOpen",
		"(JLjava/lang/String;[Ljava/lang/Object;)Lorg/postgresql/pljava/internal/Portal;",
		Java_org_postgresql_pljava_internal_ExecutionPlan__1cursorOpen
		},
		{
		"_isCursorPlan",
		"()Z",
		Java_org_postgresql_pljava_internal_ExecutionPlan__1isCursorPlan
		},
		{
		"_execute",
		"(J[Ljava/lang/Object;I)I",
		Java_org_postgresql_pljava_internal_ExecutionPlan__1execute
		},
		{
		"_prepare",
		"(Ljava/lang/String;[Lorg/postgresql/pljava/internal/Oid;)V",
		Java_org_postgresql_pljava_internal_ExecutionPlan__1prepare
		},
		{
		"_invalidate",
		"()V",
		Java_org_postgresql_pljava_internal_ExecutionPlan__1invalidate
		},
		{ 0, 0, 0 }};

	JNIEnv* env = (JNIEnv*)PG_GETARG_POINTER(0);

	s_ExecutionPlan_class = (*env)->NewGlobalRef(
				env, PgObject_getJavaClass(env, "org/postgresql/pljava/internal/ExecutionPlan"));

	PgObject_registerNatives2(env, s_ExecutionPlan_class, methods);

	s_ExecutionPlanClass = NativeStructClass_alloc("type.ExecutionPlan");
	s_ExecutionPlanClass->JNISignature   = "Lorg/postgresql/pljava/internal/ExecutionPlan;";
	s_ExecutionPlanClass->javaTypeName   = "org.postgresql.pljava.internal.ExecutionPlan";
	s_ExecutionPlan = TypeClass_allocInstance(s_ExecutionPlanClass, InvalidOid);

	Type_registerJavaType("org.postgresql.pljava.internal.ExecutionPlan", ExecutionPlan_obtain);
	PG_RETURN_VOID();
}

static bool coerceObjects(JNIEnv* env, void* ePlan, jobjectArray jvalues, Datum** valuesPtr, char** nullsPtr)
{
	char*  nulls = 0;
	Datum* values = 0;

	int count = SPI_getargcount(ePlan);
	if((jvalues == 0 && count != 0)
	|| (jvalues != 0 && count != (*env)->GetArrayLength(env, jvalues)))
	{
		Exception_throw(env, ERRCODE_PARAMETER_COUNT_MISMATCH,
			"Number of values does not match number of arguments for prepared plan");
		return false;
	}

	if(count > 0)
	{
		int idx;
		values = (Datum*)palloc(count * sizeof(Datum));
		for(idx = 0; idx < count; ++idx)
		{
			Oid typeId = SPI_getargtypeid(ePlan, idx);
			Type type = Type_fromOid(typeId);
			jobject value = (*env)->GetObjectArrayElement(env, jvalues, idx);
			if(value != 0)
			{
				values[idx] = Type_coerceObject(type, env, value);
				(*env)->DeleteLocalRef(env, value);
			}
			else
			{
				values[idx] = 0;
				if(nulls == 0)
				{
					nulls = (char*)palloc(count+1);
					memset(nulls, ' ', count);	/* all values non-null initially */
					nulls[count] = 0;
					*nullsPtr = nulls;
				}
				nulls[idx] = 'n';
			}
		}
	}
	*valuesPtr = values;
	*nullsPtr = nulls;
	return true;
}

/****************************************
 * JNI methods
 ****************************************/
/*
 * Class:     org_postgresql_pljava_internal_ExecutionPlan
 * Method:    _cursorOpen
 * Signature: (JLjava/lang/String;[Ljava/lang/Object;)Lorg/postgresql/pljava/internal/Portal;
 */
JNIEXPORT jobject JNICALL
Java_org_postgresql_pljava_internal_ExecutionPlan__1cursorOpen(JNIEnv* env, jobject _this, jlong threadId, jstring cursorName, jobjectArray jvalues)
{
	STACK_BASE_VARS
	void* ePlan;
	jobject jportal = 0;
	PLJAVA_ENTRY_FENCE(0)

	ePlan = NativeStruct_getStruct(env, _this);
	if(ePlan == 0)
		return 0;

	STACK_BASE_PUSH(threadId)
	PG_TRY();
	{
		Datum*  values  = 0;
		char*   nulls   = 0;
		if(coerceObjects(env, ePlan, jvalues, &values, &nulls))
		{
			Portal portal;
			char* name = 0;
			if(cursorName != 0)
				name = String_createNTS(env, cursorName);

			Backend_assertConnect();
			portal = SPI_cursor_open(
				name, ePlan, values, nulls, Function_isCurrentReadOnly());
			if(name != 0)
				pfree(name);
			if(values != 0)
				pfree(values);
			if(nulls != 0)
				pfree(nulls);
		
			jportal = Portal_create(env, portal);
		}
		STACK_BASE_POP()
	}
	PG_CATCH();
	{
		STACK_BASE_POP()
		Exception_throw_ERROR(env, "SPI_cursor_open");
	}
	PG_END_TRY();
	return jportal;
}

/*
 * Class:     org_postgresql_pljava_internal_ExecutionPlan
 * Method:    _isCursorPlan
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_org_postgresql_pljava_internal_ExecutionPlan__1isCursorPlan(JNIEnv* env, jobject _this)
{
	void* ePlan;
	bool result = false;
	PLJAVA_ENTRY_FENCE(false)

	ePlan = NativeStruct_getStruct(env, _this);
	if(ePlan == 0)
		return 0;

	PG_TRY();
	{
		Backend_assertConnect();
		result = SPI_is_cursor_plan(ePlan);
	}
	PG_CATCH();
	{
		Exception_throw_ERROR(env, "SPI_is_cursor_plan");
	}
	PG_END_TRY();
	return result;
}

/*
 * Class:     org_postgresql_pljava_internal_ExecutionPlan
 * Method:    _execute
 * Signature: (J[Ljava/lang/Object;I)V
 */
JNIEXPORT jint JNICALL
Java_org_postgresql_pljava_internal_ExecutionPlan__1execute(JNIEnv* env, jobject _this, jlong threadId, jobjectArray jvalues, jint count)
{
	STACK_BASE_VARS
	void* ePlan;
	jint result = 0;
	PLJAVA_ENTRY_FENCE(0)
	
	ePlan = NativeStruct_getStruct(env, _this);
	if(ePlan == 0)
		return 0;

	STACK_BASE_PUSH(threadId)
	Backend_pushJavaFrame(env);
	PG_TRY();
	{
		Datum* values = 0;
		char*  nulls  = 0;
		if(coerceObjects(env, ePlan, jvalues, &values, &nulls))
		{
			Backend_assertConnect();
			result = (jint)SPI_execute_plan(
				ePlan, values, nulls, Function_isCurrentReadOnly(), (int)count);
			if(result < 0)
				Exception_throwSPI(env, "execute_plan", result);

			if(values != 0)
				pfree(values);
			if(nulls != 0)
				pfree(nulls);
		}
		Backend_popJavaFrame(env);
		STACK_BASE_POP()
	}
	PG_CATCH();
	{
		Backend_popJavaFrame(env);
		STACK_BASE_POP()
		Exception_throw_ERROR(env, "SPI_execute_plan");
	}
	PG_END_TRY();

	return result;
}

/*
 * Class:     org_postgresql_pljava_internal_ExecutionPlan
 * Method:    _prepare
 * Signature: ()V;
 */
JNIEXPORT void JNICALL
Java_org_postgresql_pljava_internal_ExecutionPlan__1prepare(JNIEnv* env, jobject _this, jstring jcmd, jobjectArray paramTypes)
{
	PLJAVA_ENTRY_FENCE_VOID
	PG_TRY();
	{
		char* cmd;
		void* ePlan;
		int paramCount = 0;
		Oid* paramOids = 0;

		if(paramTypes != 0)
		{
			paramCount = (*env)->GetArrayLength(env, paramTypes);
			if(paramCount > 0)
			{
				int idx;
				paramOids = (Oid*)palloc(paramCount * sizeof(Oid));
				for(idx = 0; idx < paramCount; ++idx)
				{
					jobject joid = (*env)->GetObjectArrayElement(env, paramTypes, idx);
					paramOids[idx] = Oid_getOid(env, joid);
					(*env)->DeleteLocalRef(env, joid);
				}
			}
		}

		cmd   = String_createNTS(env, jcmd);
		Backend_assertConnect();
		ePlan = SPI_prepare(cmd, paramCount, paramOids);
		pfree(cmd);

		if(ePlan == 0)
			Exception_throwSPI(env, "prepare", SPI_result);
		else
		{
			NativeStruct_setPointer(env, _this, SPI_saveplan(ePlan));
			SPI_freeplan(ePlan);	/* Get rid of the original, nobody can see it anymore */
		}
	}
	PG_CATCH();
	{
		Exception_throw_ERROR(env, "SPI_prepare");
	}
	PG_END_TRY();
}

/*
 * Class:     org_postgresql_pljava_internal_ExecutionPlan
 * Method:    _invalidate
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_org_postgresql_pljava_internal_ExecutionPlan__1invalidate(JNIEnv* env, jobject _this)
{
	void* ePlan;
	PLJAVA_ENTRY_FENCE_VOID

	/* The plan is not cached as a normal NativeStruct since its made
	 * persistent.
	 */
	ePlan = NativeStruct_getStruct(env, _this);
	if(ePlan == 0)
		return;

	PG_TRY();
	{
		NativeStruct_setPointer(env, _this, 0);
		SPI_freeplan(ePlan);
	}
	PG_CATCH();
	{
		Exception_throw_ERROR(env, "SPI_freeplan");
	}
	PG_END_TRY();
}
