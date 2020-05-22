#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include <CDctReportCollector.hpp>
#include "AaSysLogPlugin.hpp"
#include <string.h>
#include <StubIfAaSysCom.hpp>
#include "CByteAligner.hpp"
#include "../../../../I_Interface/Public/Application/Lte_Env/SC_LOM/Interfaces/lom/measurement/msgs/MessageId_LomMeasurement.h"
#include "../../../../I_Interface/Private/Application/Lte_Env/SC_LTEL2/Definitions/LTEL2_common.h"

static const u32 CELL_ID = 0xABC;
#define ITEM_COLLECT_COUNT_DLSCHDATA2   0xA
#define UT_SCELL_IDENTY_HACK    0x1B

u32 numOfScheduledTTI;

typedef enum ESubFrameToSchedule
{
    ESubFrameToSchedule_EverySubframe = 0,
    ESubFrameToSchedule_EvenSubframeOnly
} ETtiToSchedule;


class DctCollectorUtMessenger
{
public:
    void* m_reportMsgHandle;
    void* m_initReqHandle;
    void* m_reportRequestHandle;

    DctCollectorUtMessenger()
    {
        m_reportMsgHandle = GLO_NULL;
        m_initReqHandle = GLO_NULL;
        m_reportRequestHandle = GLO_NULL;
    }
    ~DctCollectorUtMessenger()
    {
        if(m_reportMsgHandle)AaSysComMsgDestroy(&m_reportMsgHandle);
        if(m_initReqHandle)AaSysComMsgDestroy(&m_initReqHandle);
        if(m_reportRequestHandle)AaSysComMsgDestroy(&m_reportRequestHandle);
    }

    void* createLteBrowserReportInd(const EBrowserObjectName objectName, const u32 actualSize, const u32 itemCollectCount,
                                    ESubFrameToSchedule scheduling,const TBoolean isSCellExpected = GLO_FALSE)
    {
        u32 totalSize = sizeof(TDataCount) * itemCollectCount + (actualSize * itemCollectCount);
        u32 msgSize = sizeof(LteBrowserReportInd) - sizeof(TDynamicData) + totalSize;
#ifdef KEPLER_LRC
        m_reportMsgHandle = AaSysComMsgCreate(LTE_BROWSER_REPORT_IND_MSG,
            (TAaSysComMsgSize) CByteAligner::roundUpToDiv4(msgSize ),
            0xFFFFFFFF);
#else
        m_reportMsgHandle = AaSysComMsgCreateX( LTE_BROWSER_REPORT_IND_MSG, (TAaSysComMsgSize) CByteAligner::roundUpToDiv4( msgSize ),
            0xFFFFFFFF, EAaSysComMtm_Basic, 0xFFFFFFFF );
#endif
        LteBrowserReportInd* const messageToBeSent = (LteBrowserReportInd*) AaSysComMsgGetPayload( m_reportMsgHandle );
        messageToBeSent->reportName = objectName;
        messageToBeSent->itemCount = itemCollectCount;
        messageToBeSent->totalSize = totalSize;

        u32* resultPtr = messageToBeSent->dynamicData;
        for(u32 i=0;i<itemCollectCount;i++)
        {
            *resultPtr = (TDataCount)actualSize;
            resultPtr++;
        }

        /* following expected results are according to the PUCCH response value defined in AckResultsForPucchAckTestModel.hpp */
        TDlAckResult expectedResults[2][10] = {
                                               {0xA,0xF,0x6,0xF,0x6,0xF,0xF,0xA,0x9,0xF},  // pCell HARQ ACK/NACK results for 10 subframes
                                               {0x9,0xF,0xF,0x6,0xA,0x9,0xF,0xA,0xF,0xA}   // sCell HARQ ACK/NACK results for 10 subframes
                                               };

        SDctDlschData2* tmDctDlData2 = (SDctDlschData2*) resultPtr;
        for(u32 i=0;i<itemCollectCount;i++)
        {
            tmDctDlData2->cellId = (isSCellExpected) ? (CELL_ID+50) : CELL_ID;
            tmDctDlData2->sfn = ( ESubFrameToSchedule_EverySubframe == scheduling || (scheduling == ESubFrameToSchedule_EvenSubframeOnly && (i%2==0)) ) ? 1023 : 0xFFFFFFFF;
            tmDctDlData2->subFrameCounter =  ( ESubFrameToSchedule_EverySubframe == scheduling || (scheduling == ESubFrameToSchedule_EvenSubframeOnly && (i%2==0)) ) ? i:0xFFFFFFFF;
            tmDctDlData2->paddingNumberOfAckNack1 = isSCellExpected;

            if( ESubFrameToSchedule_EverySubframe == scheduling || (scheduling == ESubFrameToSchedule_EvenSubframeOnly && (i%2==0)) )
            {
                tmDctDlData2->numberOfAckNack = 1;
                tmDctDlData2->ackNackResults[0].dctG6M29 = (isSCellExpected) ? (expectedResults[1][i]) : EHarqResultG6M29_Ack;;
            }
            else
            {
                tmDctDlData2->numberOfAckNack = 0;
            }
            tmDctDlData2++;
        }

        return m_reportMsgHandle;
    }

    void* createDctInitReq()
    {
#ifdef KEPLER_LRC
        m_initReqHandle = AaSysComMsgCreate(RLC_DCT_INIT_REQ,
            (TAaSysComMsgSize) CByteAligner::roundUpToDiv4(sizeof(RLC_DctInitReq)),
            0xFFFFFFFF);
#else
        m_initReqHandle = AaSysComMsgCreateX( RLC_DCT_INIT_REQ,
            (TAaSysComMsgSize) CByteAligner::roundUpToDiv4( sizeof(RLC_DctInitReq) ), 0xFFFFFFFF, EAaSysComMtm_Basic,
            0xFFFFFFFF );
#endif
        RLC_DctInitReq* const messageToBeSent = (RLC_DctInitReq*) AaSysComMsgGetPayload( m_initReqHandle );
        messageToBeSent->measuredCellId = CELL_ID;
        return m_initReqHandle;
    }

    void* createRlcOverallReportReq(const EBrowserObjectName objectName)
    {
#ifdef KEPLER_LRC
        m_reportRequestHandle = AaSysComMsgCreate(RLC_DCT_OVER_ALL_REPORT_REQ,
            (TAaSysComMsgSize) CByteAligner::roundUpToDiv4(sizeof(RLC_DctOverAllReportReq)),
            0xFFFFFFFF);
#else
        m_reportRequestHandle = AaSysComMsgCreateX( RLC_DCT_OVER_ALL_REPORT_REQ,
            (TAaSysComMsgSize) CByteAligner::roundUpToDiv4( sizeof(RLC_DctOverAllReportReq) ), 0xFFFFFFFF, EAaSysComMtm_Basic,
            0xFFFFFFFF );
#endif
        RLC_DctOverAllReportReq* const messageToBeSent = (RLC_DctOverAllReportReq*) AaSysComMsgGetPayload( m_reportRequestHandle );
        messageToBeSent->dctMeasName = objectName;
        return m_reportRequestHandle;
    }
};

class DctMessageHandler
{
public:

    static void MyMessageSend(void ** msg)
    {
        const TAaSysComMsgId msgId = AaSysComMsgGetId( *msg );
        switch( msgId )
        {
            case RLC_DCT_DLSCH_DATA_2_TM_REPORT_RESP:
            {
                RLC_DctDlSchData2TmReportResp* resp = (RLC_DctDlSchData2TmReportResp*) AaSysComMsgGetPayload( *msg );
                if(resp->hasPucchFormat1A)
                {
                    CHECK_EQUAL(numOfScheduledTTI,resp->dctDlSchData2Reports[0].cumulativeG6M29_Ack);
                }
                if(resp->hasPucchFormat1B)
                {
                    CHECK_EQUAL(GLO_FALSE,resp->failDetected);
                }
                CHECK_EQUAL(numOfScheduledTTI,resp->dctDlSchData2Reports[0].numOfDctDlSchData2TmResults);
                break;
            }
            case RLC_DCT_ULSCH_DATA_1_TM_REPORT_RESP:
                break;
            default:
                AaSysLogPrint( EAaSysLogSeverityLevel_Error,"TestCDctReportCollector,unknown DCT report type received,%d",msgId);
                break;
        }
        AaSysComMsgDestroy (msg);
    }
};

TEST_GROUP(TestCDctReportCollector)
{
    DctMessageHandler m_myMsgHandler;
    CDctReportCollector* m_collector;
    DctCollectorUtMessenger m_messenger;
    static const TNumberOfItems m_dlSchData1RecordCount = 10;
    SDctDlschData1 dlSchData1;
    static const u32 cellIndex1 = 0;
    static const u32 cellIndex2 = 1;

    void setup()
    {
        numOfScheduledTTI = 0;
        UT_PTR_SET(AaSysComMsgSend_stub, &m_myMsgHandler.MyMessageSend);
        SYSLOG_PLUGIN_DISABLE_INFOLOGS();
        mock("PS").disable();
        m_collector = new CDctReportCollector();
        memset(&dlSchData1, 0, sizeof(dlSchData1));
    }
    void teardown()
    {
        delete m_collector;
    }

    void fakeDctInitReqSendingToCollector()
    {
        void* handle = m_messenger.createDctInitReq();
        m_collector->initCellIds(handle);
    }

    void sendReportRequest(const EBrowserObjectName objectName)
    {
        void* msg = m_messenger.createRlcOverallReportReq(objectName);
        m_collector->giveReport(msg);
    }

    void initializeDlSchData1RecordPointers()
    {
        dlSchData1.cellId = 1;
        dlSchData1.numberOfPdus = 0;

        for(u32 i = 0; i < m_dlSchData1RecordCount; i++)
        {
            m_collector->m_resultPointer[i] = reinterpret_cast<void*>(&dlSchData1);
        }
    }
};

TEST(TestCDctReportCollector, collectUlSch1Tm)
{
    m_collector->init();
    SDctUlsch1 ulsch1;
    m_collector->dctUlsch1TmResults.numOfDctUlsch1TmReports = 0;
    memset(&ulsch1, 0, sizeof(SDctUlsch1));
    m_collector->collectUlSch1Tm(&ulsch1);
    m_collector->collectUlSch1Tm(0);
    CHECK_EQUAL(0, m_collector->dctUlsch1TmResults.dctUlSch1TmReports[0].numOfDctUlsch1TmResults);
    ulsch1.numberOfUsers = 1;
    m_collector->collectUlSch1Tm(&ulsch1);
    CHECK_EQUAL(1, m_collector->dctUlsch1TmResults.dctUlSch1TmReports[0].numOfDctUlsch1TmResults);
}

TEST(TestCDctReportCollector, collectDlSch1Tm)
{
    m_collector->init();
    SDctDlsch1 dlsch1;
    memset(&dlsch1, 0, sizeof(SDctDlsch1));
    m_collector->collectDlSch1Tm(&dlsch1);
    m_collector->collectDlSch1Tm(0);
    CHECK_EQUAL(0, m_collector->dctDlsch1TmResults.dctDlsch1Reports[0].numOfDlsch1TmResults);
    dlsch1.numOfDctDlsch1Results = 1;
    dlsch1.dctDlsch1Results[0].dctG5M6 = 1;
    m_collector->collectDlSch1Tm(&dlsch1);
    CHECK_EQUAL(1, m_collector->dctDlsch1TmResults.dctDlsch1Reports[0].numOfDlsch1TmResults);
    CHECK_EQUAL(1, m_collector->dctDlsch1TmResults.dctDlsch1Reports[0].cumulativeDctG5M6);
}

TEST(TestCDctReportCollector,collectDlSchData2Tm_WithItemCollectCount_10_forPCell)
{
    numOfScheduledTTI = 10;
    fakeDctInitReqSendingToCollector();
    void* msgHandle = m_messenger.createLteBrowserReportInd(EBrowserObjectName_DctDlschData2Tm,sizeof(SDctDlschData2),ITEM_COLLECT_COUNT_DLSCHDATA2,ESubFrameToSchedule_EverySubframe);
    m_collector->reportIndReceived((LteBrowserReportInd *)AaSysComMsgGetPayload(msgHandle), AaSysComMsgGetPayloadSize(msgHandle));
    sendReportRequest(EBrowserObjectName_DctDlschData2Tm);
}

TEST(TestCDctReportCollector,collectDlSchData2Tm_WithItemCollectCount_10_forSCell)
{
    numOfScheduledTTI = 10;
    fakeDctInitReqSendingToCollector();
    void* msgHandle = m_messenger.createLteBrowserReportInd(EBrowserObjectName_DctDlschData2Tm,sizeof(SDctDlschData2),ITEM_COLLECT_COUNT_DLSCHDATA2,ESubFrameToSchedule_EverySubframe,GLO_TRUE);
    m_collector->reportIndReceived((LteBrowserReportInd *)AaSysComMsgGetPayload(msgHandle), AaSysComMsgGetPayloadSize(msgHandle));
    sendReportRequest(EBrowserObjectName_DctDlschData2Tm);
}

TEST(TestCDctReportCollector,collectDlSchData2Tm_WithItemCollectCount_10_forSCell_schedule_even_subframes_only)
{
    numOfScheduledTTI = 5;
    fakeDctInitReqSendingToCollector();
    void* msgHandle = m_messenger.createLteBrowserReportInd(EBrowserObjectName_DctDlschData2Tm,sizeof(SDctDlschData2),ITEM_COLLECT_COUNT_DLSCHDATA2,ESubFrameToSchedule_EvenSubframeOnly,GLO_TRUE);
    m_collector->reportIndReceived((LteBrowserReportInd *)AaSysComMsgGetPayload(msgHandle), AaSysComMsgGetPayloadSize(msgHandle));
    sendReportRequest(EBrowserObjectName_DctDlschData2Tm);
}

TEST(TestCDctReportCollector, collectUlSchData1Tm)
{
    m_collector->init();
    SDctUlschData1 ulschData1;
    memset(&ulschData1, 0, sizeof(SDctUlschData1));
    m_collector->collectUlSchData1Tm(&ulschData1);
    m_collector->collectUlSchData1Tm(0);
    CHECK_EQUAL(0, m_collector->dctUlSchData1TmResults.dctUlschData1Reports[0].numOfDctUlSchData1Results);
    ulschData1.numOfDctUlschData1Results = 1;
    ulschData1.dctUlschData1Results[0].dctG3M22 = 1;
    m_collector->collectUlSchData1Tm(&ulschData1);
    CHECK_EQUAL(1, m_collector->dctUlSchData1TmResults.dctUlschData1Reports[0].numOfDctUlSchData1Results);
    CHECK_EQUAL(1, m_collector->dctUlSchData1TmResults.dctUlschData1Reports[0].cumulativeG3M22);
}

TEST(TestCDctReportCollector, collectUlLevL1CellTm)
{
    m_collector->init();
    SDctUlLevL1Cell ulLevL1Cell;
    memset(&ulLevL1Cell, 0, sizeof(SDctUlLevL1Cell));
    m_collector->collectUlLevL1CellTm(&ulLevL1Cell);
    m_collector->collectUlLevL1CellTm(0);
    CHECK_EQUAL(1, m_collector->dctUlLevL1CellTmResults.numOfDctUlLevL1CellTmReport);
}

TEST(TestCDctReportCollector, collectUlschData2Tm)
{
    m_collector->init();
    SDctUlschData2 ulschData2;
    memset(&ulschData2, 0, sizeof(SDctUlschData2));
    m_collector->collectUlschData2Tm(&ulschData2);
    m_collector->collectUlschData2Tm(0);
    CHECK_EQUAL(0, m_collector->dctUlSchData2TmResults.dctUlSchData2Reports[0].numOfDctUlSchData2Results);
    ulschData2.numberOfDctUlschData2Results = 1;
    ulschData2.SDctUlschData2Results[0].dctG3M23 = 1;
    m_collector->collectUlschData2Tm(&ulschData2);
    CHECK_EQUAL(1, m_collector->dctUlSchData2TmResults.dctUlSchData2Reports[0].numOfDctUlSchData2Results);
    CHECK_EQUAL(1, m_collector->dctUlSchData2TmResults.dctUlSchData2Reports[0].cumulativeG3M23);
}

TEST(TestCDctReportCollector, collectUlRlcAmPdu)
{
    m_collector->init();
    SDctUlRlcAmPdu ulRlcAmPdu;
    memset(&ulRlcAmPdu, 0, sizeof(SDctUlRlcAmPdu));
    ulRlcAmPdu.rbsResults[0].ueId = 1;

    SYSLOG_EXPECT_ERROR("CDctReportCollector:collectUlRlcAmPdu report has numberOfUsers:");
    ulRlcAmPdu.numberOfUsers = 0;
    m_collector->collectUlRlcAmPdu(&ulRlcAmPdu,EBrowserObjectName_DctUlRlcAmPdu);
    CHECK_EQUAL(0, m_collector->numOfDctUlRlcAmPduResults);

    ulRlcAmPdu.numberOfUsers = 1;
    m_collector->collectUlRlcAmPdu(&ulRlcAmPdu,EBrowserObjectName_DctUlRlcAmPdu);
    CHECK_EQUAL(1, m_collector->numOfDctUlRlcAmPduResults);
}

TEST(TestCDctReportCollector, collectUlRlcUmPdu)
{
    m_collector->init();
    SDctUlRlcUmPdu ulRlcUmPdu;
    memset(&ulRlcUmPdu, 0, sizeof(SDctUlRlcUmPdu));
    ulRlcUmPdu.numberOfUsers = 1;
    ulRlcUmPdu.rbsResults[0].ueId = 1;
    m_collector->collectUlRlcUmPdu(&ulRlcUmPdu,EBrowserObjectName_DctDlRlcUmPdu);
    CHECK_EQUAL(0, m_collector->numOfDctUlRlcAmPduResults);
    CHECK_EQUAL(1, m_collector->numOfDctUlRlcUmPduResults);
}

TEST(TestCDctReportCollector, collectUlSch1)
{
    m_collector->init();
    SDctUlsch1 ulSch1;
    memset(&ulSch1, 0, sizeof(SDctUlsch1));
    ulSch1.SDctUlsch1Results[0].ueId = 1;

    SYSLOG_EXPECT_ERROR("CDctReportCollector::collectUlSch1() report has numberOfUsers:");
    ulSch1.numberOfUsers = 0;
    m_collector->collectUlSch1(&ulSch1,EBrowserObjectName_DctUlsch1);
    CHECK_EQUAL(0, m_collector->numOfDctUlsch1Results);

    ulSch1.numberOfUsers = 1;
    m_collector->collectUlSch1(&ulSch1,EBrowserObjectName_DctUlsch1);
    CHECK_EQUAL(1, m_collector->numOfDctUlsch1Results);
}

TEST(TestCDctReportCollector, collectDlRlcAmPdu)
{
    m_collector->init();
    SDctDlRlcAmPdu dlRlcAmPdu;
    memset(&dlRlcAmPdu, 0, sizeof(SDctDlRlcAmPdu));
    dlRlcAmPdu.rbsResults[0].ueId = 1;

    SYSLOG_EXPECT_ERROR("CDctReportCollector:collectDlRlcAmPdu report has numberOfUsers:");
    dlRlcAmPdu.numberOfUsers = 0;
    m_collector->collectDlRlcAmPdu(&dlRlcAmPdu,EBrowserObjectName_DctDlRlcAmPdu);
    CHECK_EQUAL(0, m_collector->numOfDctDlRlcAmPduResults);

    dlRlcAmPdu.numberOfUsers = 1;
    m_collector->collectDlRlcAmPdu(&dlRlcAmPdu,EBrowserObjectName_DctDlRlcAmPdu);
    CHECK_EQUAL(1, m_collector->numOfDctDlRlcAmPduResults);
}

TEST(TestCDctReportCollector, collectDlRlcUmPdu)
{
    m_collector->init();
    SDctDlRlcUmPdu dlRlcUmPdu;
    memset(&dlRlcUmPdu, 0, sizeof(SDctDlRlcUmPdu));
    dlRlcUmPdu.rbsResults[0].ueId = 1;

    SYSLOG_EXPECT_ERROR("CDctReportCollector:collectDlRlcUmPdu report has numberOfUsers:");
    dlRlcUmPdu.numberOfUsers = 0;
    m_collector->collectDlRlcUmPdu(&dlRlcUmPdu,EBrowserObjectName_DctDlRlcUmPdu);
    CHECK_EQUAL(0, m_collector->numOfDctDlRlcUmPduResults);

    dlRlcUmPdu.numberOfUsers = 1;
    m_collector->collectDlRlcUmPdu(&dlRlcUmPdu,EBrowserObjectName_DctDlRlcUmPdu);
    CHECK_EQUAL(1, m_collector->numOfDctDlRlcUmPduResults);
}

TEST(TestCDctReportCollector, collectDlRlcSdu)
{
    m_collector->init();
    SDctDlRlcSdu dlRlcSdu;
    memset(&dlRlcSdu, 0, sizeof(SDctDlRlcSdu));
    dlRlcSdu.ueId = 1;
    dlRlcSdu.cellId =1;

    dlRlcSdu.numOfDctDlRlcSduResults = 2;
    m_collector->collectDlRlcSdu(&dlRlcSdu,EBrowserObjectName_DctDlRlcSdu);
    CHECK_EQUAL(1, m_collector->numOfDctDlRlcSduResults);
}

TEST(TestCDctReportCollector, whenTooManyEmptyReportsAreSentWithSameSequenceNumber_thenDctCollectorPrintsError)
{
    initializeDlSchData1RecordPointers();

    SYSLOG_EXPECT_ERROR("CDctReportCollector::checkIfMaximumNumberOfEmptyReportsExeeded");

    const u32 sequenceNumber = 1;
    for(u32 i = 0; i < NUM_OF_UE_GROUPS_PER_POOL + 1; i++)
    {
        m_collector->ensureEmptyDlSchData1ReportsAreReceivedOnlyFromPCellPool(cellIndex1, m_dlSchData1RecordCount, sequenceNumber);
    }
}

TEST(TestCDctReportCollector, whenManyEmptyReportsAreSentWithDifferentSequenceNumber_thenErrorIsNotPrinted)
{
    initializeDlSchData1RecordPointers();

    u32 sequenceNumber = 1;
    for(u32 i = 0; i < NUM_OF_UE_GROUPS_PER_POOL * 10; i++)
    {
        m_collector->ensureEmptyDlSchData1ReportsAreReceivedOnlyFromPCellPool(cellIndex1, m_dlSchData1RecordCount, sequenceNumber);

        if(i % NUM_OF_UE_GROUPS_PER_POOL)
        {
            sequenceNumber++;
        }
    }
}

TEST(TestCDctReportCollector, whenEmptyReportsAreSentForDifferentCells_thenErrorIsNotPrinted)
{
    initializeDlSchData1RecordPointers();
    const u32 sequenceNumber = 1;

    for(u32 i = 0; i < NUM_OF_UE_GROUPS_PER_POOL; i++)
    {
        m_collector->ensureEmptyDlSchData1ReportsAreReceivedOnlyFromPCellPool(cellIndex1, m_dlSchData1RecordCount, sequenceNumber);
    }

    for(u32 i = 0; i < NUM_OF_UE_GROUPS_PER_POOL; i++)
    {
        m_collector->ensureEmptyDlSchData1ReportsAreReceivedOnlyFromPCellPool(cellIndex2, m_dlSchData1RecordCount, sequenceNumber);
    }
}

TEST(TestCDctReportCollector, getCellIndex)
{
    TCellId cellId1 = 123, cellId2 = 345, cellId3 = 678;
    const u32 cellIndex1 = m_collector->getCellIndex(cellId1);
    const u32 cellIndex2 = m_collector->getCellIndex(cellId2);

    CHECK_EQUAL(cellIndex1, m_collector->getCellIndex(cellId1));
    CHECK_EQUAL(cellIndex2, m_collector->getCellIndex(cellId2));
    CHECK_EQUAL(CDctReportCollector::INVALID_VALUE, m_collector->getCellIndex(cellId3));
}
