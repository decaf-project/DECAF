/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*
 * Array handling.
 */
#ifndef _DALVIK_OO_ARRAY
#define _DALVIK_OO_ARRAY

/* width of an object reference, for arrays of objects */
#define kObjectArrayRefWidth    sizeof(Object*)

/*
 * Find a matching array class.  If it doesn't exist, create it.
 *
 * "descriptor" looks like "[I".
 *
 * "loader" should be the defining class loader for the elements held
 * in the array.
 */
ClassObject* dvmFindArrayClass(const char* descriptor, Object* loader);

/*
 * Find the array class for the specified class.  If "elemClassObj" is the
 * class "Foo", this returns the class object for "[Foo".
 */
ClassObject* dvmFindArrayClassForElement(ClassObject* elemClassObj);

/*
 * Allocate space for a new array object.
 *
 * "allocFlags" determines whether the new object will be added to the
 * "tracked alloc" table.
 *
 * Returns NULL with an exception raised if allocation fails.
 */
ArrayObject* dvmAllocArray(ClassObject* arrayClass, size_t length,
    size_t elemWidth, int allocFlags);

/*
 * Create a new array, given an array class.  The class may represent an
 * array of references or primitives.
 *
 * Returns NULL with an exception raised if allocation fails.
 */
ArrayObject* dvmAllocArrayByClass(ClassObject* arrayClass,
    size_t length, int allocFlags);

/*
 * Create a new array that holds references to members of the specified class.
 *
 * "elemClassObj" is the element type, and may itself be an array class.  It
 * may not be a primitive class.
 *
 * "allocFlags" determines whether the new object will be added to the
 * "tracked alloc" table.
 *
 * This is less efficient than dvmAllocArray(), but occasionally convenient.
 *
 * Returns NULL with an exception raised if allocation fails.
 */
ArrayObject* dvmAllocObjectArray(ClassObject* elemClassObj, size_t length,
    int allocFlags);

/*
 * Allocate an array whose members are primitives (bools, ints, etc.).
 *
 * "type" should be 'I', 'J', 'Z', etc.
 *
 * The new object will be added to the "tracked alloc" table.
 *
 * Returns NULL with an exception raised if allocation fails.
 */
ArrayObject* dvmAllocPrimitiveArray(char type, size_t length, int allocFlags);

/*
 * Allocate an array with multiple dimensions.  Elements may be Objects or
 * primitive types.
 *
 * The base object will be added to the "tracked alloc" table.
 *
 * Returns NULL with an exception raised if allocation fails.
 */
ArrayObject* dvmAllocMultiArray(ClassObject* arrayClass, int curDim,
    const int* dimensions);

/*
 * Find the synthesized object for the primitive class, generating it
 * if this is the first reference.
 */
ClassObject* dvmFindPrimitiveClass(char type);

/*
 * Verify that the object is actually an array.
 *
 * Does not verify that the object is actually a non-NULL object.
 */
INLINE bool dvmIsArray(const ArrayObject* arrayObj)
{
    return ( ((Object*)arrayObj)->clazz->descriptor[0] == '[' );
}

/*
 * Verify that the array is an object array and not a primitive array.
 *
 * Does not verify that the object is actually a non-NULL object.
 */
INLINE bool dvmIsObjectArrayClass(const ClassObject* clazz)
{
    const char* descriptor = clazz->descriptor;
    return descriptor[0] == '[' && (descriptor[1] == 'L' ||
                                    descriptor[1] == '[');
}

/*
 * Verify that the array is an object array and not a primitive array.
 *
 * Does not verify that the object is actually a non-NULL object.
 */
INLINE bool dvmIsObjectArray(const ArrayObject* arrayObj)
{
    return dvmIsObjectArrayClass(arrayObj->obj.clazz);
}

/*
 * Verify that the class is an array class.
 *
 * TODO: there may be some performance advantage to setting a flag in
 * the accessFlags field instead of chasing into the name string.
 */
INLINE bool dvmIsArrayClass(const ClassObject* clazz)
{
    return (clazz->descriptor[0] == '[');
}

/*
 * Copy the entire contents of one array of objects to another.  If the copy
 * is impossible because of a type clash, we fail and return "false".
 *
 * "dstElemClass" is the type of element that "dstArray" holds.
 */
bool dvmCopyObjectArray(ArrayObject* dstArray, const ArrayObject* srcArray,
    ClassObject* dstElemClass);

/*
 * Copy the entire contents of an array of boxed primitives into an
 * array of primitives.  The boxed value must fit in the primitive (i.e.
 * narrowing conversions are not allowed).
 */
bool dvmUnboxObjectArray(ArrayObject* dstArray, const ArrayObject* srcArray,
    ClassObject* dstElemClass);

/*
 * Returns the size of the given array object in bytes.
 */
size_t dvmArrayObjectSize(const ArrayObject *array);

/*
 * Returns the width, in bytes, required by elements in instances of
 * the array class.
 */
size_t dvmArrayClassElementWidth(const ClassObject* clazz);

#endif /*_DALVIK_OO_ARRAY*/
