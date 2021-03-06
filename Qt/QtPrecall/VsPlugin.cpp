#include "VsPlugin.h"
#include <QObject>
#include <QApplication>
#include <QDebug>
#include <QDir>
#include "VsPlugin.h"
#include "VDSound.h"

VsPlugin * VsPlugin::instance = NULL;

#define SAFE_STRING_CPY(destination, source, limit){\
    size_t sizeToCopy = (limit > 0) ? (limit -1) : 0;\
    strncpy(destination, source, sizeToCopy);\
}

#ifdef Q_WS_WIN
void __stdcall VidyoClientOutCallback
#else
void VidyoClientOutCallback
#endif
(VidyoClientOutEvent event,
              VidyoVoidPtr param, VidyoUint paramSize,
              VidyoVoidPtr data)
{
    if (VsPlugin::getInstance() == NULL)
    {
        return;
    }

    if ((event >= VIDYO_CLIENT_OUT_EVENT_MIN
        && event <= VIDYO_CLIENT_OUT_EVENT_MAX))
    {
        VidyoVoidPtr copyParam = malloc(paramSize);
        memcpy(copyParam, param, paramSize);

        if (copyParam != NULL)
        {
            if ((event >= VIDYO_CLIENT_OUT_EVENT_MIN
                && event <= VIDYO_CLIENT_OUT_EVENT_MAX))
            {
                VsPlugin::getInstance()->EmitOutEvent(event, copyParam, paramSize, data);
            }
        }
    }
    return;

}

void VsPlugin::EmitOutEvent(VidyoClientOutEvent event, VidyoVoidPtr param, VidyoUint paramSize, VidyoVoidPtr data)
{
    emit(this->OutEvent(event, param, paramSize, data));
}

void InitLogParams(VidyoClientLogParams& logParams)
{
    const unsigned int maxLogFileSize = 1000000;
    
#ifdef Q_WS_WIN
    QString folder("log\\");
#else
    QString folder = QDir::homePath()+ QString("/VsLog/");
#endif
    logParams.pathToLogDir= folder.toStdString().c_str();
    logParams.pathToDumpDir = folder.toStdString().c_str();
    logParams.pathToConfigDir = folder.toStdString().c_str();
    
    if(!QDir(folder).exists())
        QDir().mkdir(folder);
    logParams.logBaseFileName = "VidyoSample_";
    logParams.logLevelsAndCategories = "fatal error warning info@App info@AppEmcpClient info@AppEvents all@AppVcsoapClient";
    logParams.logSize = maxLogFileSize;
    
    if(!QDir(logParams.pathToLogDir).exists())
        QDir().mkdir(logParams.pathToLogDir);
    
}

void InitProfileParams(VidyoClientProfileParams& profileParams)
{
#ifdef Q_WS_WIN
    profileParams.PROGFILES_DIR = ".\\";
    profileParams.CONFIG_KEY_PATH = "SOFTWARE\\Vidyo\\Vidyo Desktop\\";

    profileParams.MYDOCS_DIR = ".\\";
    profileParams.VIDYO_DIR = ".\\";
#else
   /*profileParams.PROGFILES_DIR = "./";
    profileParams.CONFIG_KEY_PATH = "./";

    profileParams.MYDOCS_DIR = "./";
    profileParams.VIDYO_DIR = "./";*/

#endif
    profileParams.portNumber = 63460;
    profileParams.PRODUCT_NAME = "VidyoDesktop";

}

VsPlugin * VsPlugin::getInstance()
{
    return instance;
}



VsPlugin::VsPlugin(VsStatus* statusWindow):
    QThread(NULL)
{
    m_Started = false;
    const unsigned int maxLogFileSize = 1000000;
    VidyoClientLogParams logParams = {0};

    InitLogParams(logParams);

    VidyoBool retVal = VidyoClientInitialize((VidyoClientOutEventCallback)VidyoClientOutCallback, NULL, &logParams);

    QObject::connect(this, SIGNAL(started()), SLOT(OnStarted()));
    this->start();
    QObject::moveToThread(this);
    instance = this;

    m_statusWindow = statusWindow;

}

void VsPlugin::OnStarted()
{

   qRegisterMetaType<VidyoClientOutEvent>("VidyoClientOutEvent");
   qRegisterMetaType<VidyoVoidPtr>("VidyoVoidPtr");
   qRegisterMetaType<VidyoUint>("VidyoUint");

    QObject::connect(this,
        SIGNAL(OutEvent(VidyoClientOutEvent,VidyoVoidPtr,VidyoUint,VidyoVoidPtr)),
        this,
        SLOT(OnOutEvent(VidyoClientOutEvent,VidyoVoidPtr,VidyoUint,VidyoVoidPtr)),
        Qt::QueuedConnection);
}

void VsPlugin::OnOutEvent(VidyoClientOutEvent event, VidyoVoidPtr param, VidyoUint paramSize, VidyoVoidPtr data)
{
    switch (event)
        {
            case VIDYO_CLIENT_OUT_EVENT_SIGN_IN:
                {
                qDebug() << "VIDYO_CLIENT_OUT_EVENT_SIGN_IN";
                VidyoClientOutEventSignIn * eventSignIn = (VidyoClientOutEventSignIn*)param;
                if(eventSignIn)
                {
                    if(!eventSignIn->activeEid)
                    {
                        //we need to consume a license
                        //As VidyoClient just informed that we never consume a license
                        VidyoClientSendEvent(VIDYO_CLIENT_IN_EVENT_LICENSE, NULL, 0);
                    }
                }
                }
                break;
            case VIDYO_CLIENT_OUT_EVENT_CONFERENCE_ACTIVE:
                qDebug() << "conference is active";
                m_statusWindow->InCall();
                break;
            case VIDYO_CLIENT_OUT_EVENT_CONFERENCE_ENDED:
                qDebug() << "conference ended";
                m_statusWindow->NotInCall();
                break;
            case VIDYO_CLIENT_OUT_EVENT_MUTED_AUDIO_IN:
                {
                    VidyoClientOutEventMuted * muteData = (VidyoClientOutEventMuted*)param;
                    bool value = (muteData->isMuted == VIDYO_TRUE);
                    m_statusWindow->MicMute(value);
                }
                break;
            case VIDYO_CLIENT_OUT_EVENT_MUTED_VIDEO:
                 {
                    VidyoClientOutEventMuted * muteData = (VidyoClientOutEventMuted*)param;
                    bool value = (muteData->isMuted == VIDYO_TRUE);
                    m_statusWindow->VideoMute(value);
                }
                break;
    }
	if (param != NULL)
        free(param);
}

VsPlugin::~VsPlugin()
{

    if(m_Started)
        VsStop();
     VidyoClientUninitialize();
}



bool VsPlugin::VsStart()
{

    int err;
    const unsigned int maxLogFileSize = 1000000;
    VidyoClientLogParams logParams = {0};
    VidyoClientProfileParams profileParams = {0};
    int keyCode;
    int window_origin_x = 0;
    int window_origin_y = 0;
    unsigned window_width = 400;
    unsigned window_height = 300;

    InitLogParams(logParams);
    InitProfileParams(profileParams);

    VidyoRect videoRect = {window_origin_x, window_origin_y, window_width, window_height};


    // startup VidyoClient library
    if (!VidyoClientStart((VidyoClientOutEventCallback)VidyoClientOutCallback,
                          NULL,
                          &logParams,
                          NULL,
                          //                      (VidyoWindowId)frame_window,
                          &videoRect,
                          NULL,
                          &profileParams,
                          NULL))
    {
        // user already notified by VidyoClientStart()
        return false;
    }

    m_Started = true;
    return true;


}

bool VsPlugin::VsStop()
{
    VidyoClientStop();

    m_Started = false;
    return true;
}

bool VsPlugin::VsRoomLink(QStringList& roomLink)
{
 #define ROOM_LINK_INTERFACE_PORTAL_INDEX   0
 #define ROOM_LINK_INTERFACE_ROOMKEY_INDEX   1
 #define ROOM_LINK_INTERFACE_GUESTNAME_INDEX   2
 #define ROOM_LINK_INTERFACE_PIN_INDEX   3

    QString portalUri(roomLink[ROOM_LINK_INTERFACE_PORTAL_INDEX]);
    QString roomKey(roomLink[ROOM_LINK_INTERFACE_ROOMKEY_INDEX]);
    QString guestName(roomLink[ROOM_LINK_INTERFACE_GUESTNAME_INDEX]);
    QString pin(roomLink[ROOM_LINK_INTERFACE_PIN_INDEX]);

    QScopedPointer<VidyoClientInEventRoomLink> backendReq(new VidyoClientInEventRoomLink());


       SAFE_STRING_CPY((char *)backendReq->portalUri, portalUri.toAscii().data(), sizeof(backendReq->portalUri));

       SAFE_STRING_CPY((char *)backendReq->roomKey, roomKey.toAscii().data(), sizeof(backendReq->roomKey));

       SAFE_STRING_CPY((char *)backendReq->displayName, guestName.toUtf8().data(), sizeof(backendReq->displayName));

       SAFE_STRING_CPY((char *)backendReq->pin, pin.toAscii().data(), sizeof(backendReq->pin));



       backendReq->clientType = VIDYO_CLIENT_CLIENTTYPE_L;



       if(!VidyoClientSendEvent((VidyoClientInEvent)VIDYO_CLIENT_IN_EVENT_ROOM_LINK, backendReq.data(), sizeof(VidyoClientInEventRoomLink)))

       {

          return false;

       }


    return true;
}

bool VsPlugin::VsCallEnd()
{
       if(!VidyoClientSendEvent(VIDYO_CLIENT_IN_EVENT_LEAVE,NULL, 0))

       {

          return false;

       }


    return true;
}

static QString GetResourcesPath()
{
#ifdef Q_WS_WIN
    return ":/sound";
#elif defined(Q_WS_MACX)
    return QCoreApplication::applicationDirPath() + "/../Resources";
#elif defined(VIDYO_LINUX)
    return "/opt/vidyo/VidyoDesktop/resources";
#else
    return "./";
#endif
}

void VsPlugin::LoadSoundFile(VidyoClientInEventStartPrecallTestSpeaker &testSpeaker)
{
   VDSound sound;
   testSpeaker.soundData =  sound.Load(GetResourcesPath() + "/testtone.wav");
}

void VsPlugin::PrecallTest(PrecallTestType testType)
{
    switch(testType)
    {
    case TestCameraOn:
    {
        VidyoClientInEventPrecallTestCamera testCamera;
        testCamera.action = VIDYO_CLIENT_DEVICE_TEST_START;
        VidyoClientSendEvent((VidyoClientInEvent)VIDYO_CLIENT_IN_EVENT_PRECALL_TEST_CAMERA, &testCamera, sizeof(testCamera));
    }
        break;
    case TestCameraOff:
    {
        VidyoClientInEventPrecallTestCamera testCamera;
        testCamera.action = VIDYO_CLIENT_DEVICE_TEST_STOP;
        VidyoClientSendEvent((VidyoClientInEvent)VIDYO_CLIENT_IN_EVENT_PRECALL_TEST_CAMERA, &testCamera, sizeof(testCamera));
    }
        break;
    case TestMicOn:
    {
        VidyoClientInEventPrecallTestMicrophone testMic;
        testMic.action = VIDYO_CLIENT_DEVICE_TEST_START;
        VidyoClientSendEvent((VidyoClientInEvent)VIDYO_CLIENT_IN_EVENT_PRECALL_TEST_MICROPHONE, &testMic, sizeof(testMic));
    }
        break;
    case TestMicOff:
    {
        VidyoClientInEventPrecallTestMicrophone testMic;
        testMic.action = VIDYO_CLIENT_DEVICE_TEST_STOP;
        VidyoClientSendEvent((VidyoClientInEvent)VIDYO_CLIENT_IN_EVENT_PRECALL_TEST_MICROPHONE, &testMic, sizeof(testMic));
    }
        break;
    case TestSpeakerOn:
    {
        VidyoClientInEventStartPrecallTestSpeaker testSpeaker;
        LoadSoundFile(testSpeaker);
        VidyoClientSendEvent((VidyoClientInEvent)VIDYO_CLIENT_IN_EVENT_START_PRECALL_TEST_SPEAKER, &testSpeaker, sizeof(testSpeaker));
    }
        break;
    case TestSpeakerOff:
    {
        VidyoClientInEventPrecallTestMicrophone testMic;
        testMic.action = VIDYO_CLIENT_DEVICE_TEST_STOP;
        VidyoClientSendEvent((VidyoClientInEvent)VIDYO_CLIENT_IN_EVENT_STOP_PRECALL_TEST_SPEAKER, NULL, 0);
    }
        break;
    }

}


int VsPlugin::QueryEnergy()
{
    VidyoClientRequestAudioEnergyInfo backendRequest = {};
    if (VidyoClientSendRequest(VIDYO_CLIENT_REQUEST_GET_AUDIO_ENERGY_INFO,
        &backendRequest, sizeof(VidyoClientRequestAudioEnergyInfo)) != VIDYO_CLIENT_ERROR_OK)
        return 0;
    return backendRequest.capturerEnergy;
}


