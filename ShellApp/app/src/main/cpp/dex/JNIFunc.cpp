
                
            #include "JNIFunc.h"
            #include "VMInterpreter.h"
            #include "../common/Util.h"
            #include "../common/Dalvik.h"
                    
        
                
                extern "C"
                JNIEXPORT void JNICALL
                Java_com_chend_testapp_MainActivity_000241_onClick__Landroid_view_View_2(
                        JNIEnv *env, jobject instance, jobject param0)
            {
                
                LOGD("jni function start: %s", __func__);
                jclass clazz_type = (*env).GetObjectClass(instance);
                jmethodID m_onClick = (*env).GetMethodID(clazz_type, "onClick", "(Landroid/view/View;)V");
                Method methodImg = *(Method *) m_onClick;
                Method *method = &methodImg;
                // init method
                initMethodByCodeItem(method);
                
                jvalue retValue;
                dvmCallMethod(env, instance, method, &retValue, param0);
                LOGD("jni function finish: %s", __func__);
            }
                
                extern "C"
                JNIEXPORT void JNICALL
                Java_com_chend_testapp_Main2Activity_onCreate__Landroid_os_Bundle_2(
                        JNIEnv *env, jobject instance, jobject param0)
            {
                
                LOGD("jni function start: %s", __func__);
                jclass clazz_type = (*env).GetObjectClass(instance);
                jmethodID m_onCreate = (*env).GetMethodID(clazz_type, "onCreate", "(Landroid/os/Bundle;)V");
                Method methodImg = *(Method *) m_onCreate;
                Method *method = &methodImg;
                // init method
                initMethodByCodeItem(method);
                
                jvalue retValue;
                dvmCallMethod(env, instance, method, &retValue, param0);
                LOGD("jni function finish: %s", __func__);
            }
                
                extern "C"
                JNIEXPORT void JNICALL
                Java_com_chend_testapp_MainActivity_onCreate__Landroid_os_Bundle_2(
                        JNIEnv *env, jobject instance, jobject param0)
            {
                
                LOGD("jni function start: %s", __func__);
                jclass clazz_type = (*env).GetObjectClass(instance);
                jmethodID m_onCreate = (*env).GetMethodID(clazz_type, "onCreate", "(Landroid/os/Bundle;)V");
                Method methodImg = *(Method *) m_onCreate;
                Method *method = &methodImg;
                // init method
                initMethodByCodeItem(method);
                
                jvalue retValue;
                dvmCallMethod(env, instance, method, &retValue, param0);
                LOGD("jni function finish: %s", __func__);
            }