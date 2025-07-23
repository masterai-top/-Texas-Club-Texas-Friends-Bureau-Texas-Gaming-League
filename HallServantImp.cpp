#include "HallServantImp.h"
#include "servant/Application.h"
#include "LogComm.h"
#include "globe.h"
#include "JFGameHttpProto.h"
#include "CommonStruct.h"
#include "CommonCode.h"
#include "XGameComm.pb.h"
#include "XGameHttp.pb.h"
#include "CommonCode.pb.h"
#include "CommonStruct.pb.h"
#include "UserInfo.pb.h"
#include "ServiceDefine.h"
#include "Push.h"
#include "LogDefine.h"
#include "ConfigServant.h"
#include "HallServer.h"
#include "util/tc_base64.h"
#include "DataProxyProto.h"
#include <string>
#include <map>
#include "DBAgentServant.h"
#include "ServiceUtil.h"
#include "LogDefine.h"
#include "pcre.h"
#include "util/tc_hash_fun.h"
#include "TimeUtil.h"
#include "util/tc_md5.h"

#include "ServiceUtil.h"

///
using namespace std;
using namespace JFGame;
using namespace JFGameHttpProto;
using namespace XGameProto;

#define PAGE_COUNT 10

//////////////////////////////////////////////////////
void HallServantImp::initialize()
{
    //initialize servant here:
    //...
}

//////////////////////////////////////////////////////
void HallServantImp::destroy()
{
    //destroy servant here:
    //...
}

//http请求处理接口
tars::Int32 HallServantImp::doRequest(const vector<tars::Char> &reqBuf, const map<std::string, std::string> &extraInfo,
                                      vector<tars::Char> &rspBuf, tars::TarsCurrentPtr current)
{
    FUNC_ENTRY("");

    int iRet = 0;

    __TRY__

    if (reqBuf.empty())
    {
        iRet = -1;
        return iRet;
    }

    XGameHttp::THttpPackage thttpPack;

    __TRY__
    if (!reqBuf.empty())
    {
        pbToObj(reqBuf, thttpPack);
    }
    __CATCH__

    if (thttpPack.vecdata().empty())
    {
        iRet = -2;
        LOG_ERROR << " data is empty. "<< endl;
        return iRet;
    }

    int64_t ms1 = TNOWMS;

    XGameHttp::THttpPackage thttpPackRsp;
    switch (thttpPack.nmsgid())
    {
        //房间列表
        case XGameProto::ActionName::HALL_MATCH_ROOM_LIST:
        {
            HallProto::MatchRoomListReq matchRoomListReq;
            pbToObj(thttpPack.vecdata(), matchRoomListReq);

            HallProto::MatchRoomListResp matchRoomListResp;
            iRet = matchRoomList(thttpPack.stuid().luid(), matchRoomListReq, matchRoomListResp);
            matchRoomListResp.set_iresultid(iRet);
            thttpPackRsp.set_vecdata(pbToString(matchRoomListResp));
            break;
        }
        //异常
        default:
        {
            ROLLLOG_ERROR << "invalid msgid:"<< thttpPack.nmsgid() << endl;
            iRet = -101;
            return iRet;
        }
    }

    int64_t ms2 = TNOWMS;
    if ((ms2 - ms1) > COST_MS)
    {
        ROLLLOG_WARN << "@Performance, msgid:" << thttpPack.nmsgid() << ", costTime:" << (ms2 - ms1) << endl;
    }

    ROLLLOG_DEBUG << "recv msg, msgid : " << thttpPack.nmsgid() << ", iRet: " << iRet << endl;

    auto ptuid = thttpPackRsp.mutable_stuid();
    ptuid->set_luid(thttpPack.stuid().luid());
    ptuid->set_stoken(thttpPack.stuid().stoken());

    thttpPackRsp.set_iver(thttpPack.iver());
    thttpPackRsp.set_iseq(thttpPack.iseq());
    thttpPackRsp.set_nmsgid(thttpPack.nmsgid());
    //pbTobuffer(thttpPackRsp, rspBuf);
    string s;
    thttpPackRsp.SerializeToString(&s);
    std::string encode_str = ServiceUtil::base64Encode((uint8_t*)s.c_str(), s.length());
    rspBuf.insert(rspBuf.begin(), encode_str.begin(), encode_str.end());
    ROLLLOG_DEBUG << "response buff size: " << rspBuf.size() << endl;

    __CATCH__;
    FUNC_EXIT("", iRet);
    return iRet;
}

//tcp请求处理接口
tars::Int32 HallServantImp::onRequest(tars::Int64 lUin, const std::string &sMsgPack, const std::string &sCurServrantAddr,
                                      const JFGame::TClientParam &stClientParam, const JFGame::UserBaseInfoExt &stUserBaseInfo, tars::TarsCurrentPtr current)
{
    int iRet = 0;

    __TRY__

    ROLLLOG_DEBUG << "recv msg, uid : " << lUin << ", addr : " << stClientParam.sAddr << endl;

    HallServant::async_response_onRequest(current, 0);

    XGameComm::TPackage pkg;
    pbToObj(sMsgPack, pkg);
    if (pkg.vecmsghead_size() == 0)
    {
        ROLLLOG_DEBUG << "package empty, uid: " << lUin << endl;
        return -1;
    }

    ROLLLOG_DEBUG << "recv msg, uid : " << lUin << ", msg : " << logPb(pkg) << endl;

    for (int i = 0; i < pkg.vecmsghead_size(); ++i)
    {
        int64_t ms1 = TNOWMS;

        auto &head = pkg.vecmsghead(i);
        switch (head.nmsgid())
        {
        case XGameProto::ActionName::USER_UPDATE_USER_INFO:
        {
            UserInfoProto::UpdateUserInfoReq updateUserInfoReq;
            pbToObj(pkg.vecmsgdata(i), updateUserInfoReq);
            iRet = onUpdateUserInfo(pkg, updateUserInfoReq, stUserBaseInfo, sCurServrantAddr);
            break;
        }
        case XGameProto::ActionName::USER_GET_USER_BASIC:
        {
            UserInfoProto::GetUserBasicReq getUserBasicReq;
            pbToObj(pkg.vecmsgdata(i), getUserBasicReq);
            iRet = onGetUserBasic(pkg, getUserBasicReq, sCurServrantAddr, current);
            break;
        }
        case XGameProto::ActionName::USER_LIST_USER_BASIC:
        {
            UserInfoProto::ListUserBasicReq listUserBasicReq;
            pbToObj(pkg.vecmsgdata(i), listUserBasicReq);
            iRet = onListUserBasic(pkg, listUserBasicReq, false, sCurServrantAddr, current);
            break;
        }
        case XGameProto::ActionName::HALL_MATCH_ROOM_LIST:
        {
            HallProto::MatchRoomListReq matchRoomListReq;
            pbToObj(pkg.vecmsgdata(i), matchRoomListReq);
            iRet = onMatchRoomList(pkg, matchRoomListReq, sCurServrantAddr, current);
            break;
        }
        case XGameProto::ActionName::USER_LIST_USER_ADDRESS:
        {
            iRet = onListUserAddress(pkg, sCurServrantAddr, current);
            break;
        }
        case XGameProto::ActionName::USER_ADD_USER_ADDRESS:
        {
            UserInfoProto::AddUserAddressReq addUserAddressReq;
            pbToObj(pkg.vecmsgdata(i), addUserAddressReq);
            iRet = onAddUserAddress(pkg, addUserAddressReq, sCurServrantAddr, current);
            break;
        }
        case XGameProto::ActionName::USER_UPDATE_USER_ADDRESS:
        {
            UserInfoProto::UpdateUserAddressReq updateUserAddressReq;
            pbToObj(pkg.vecmsgdata(i), updateUserAddressReq);
            iRet = onUpdateUserAddress(pkg, updateUserAddressReq, sCurServrantAddr, current);
            break;
        }
        case XGameProto::ActionName::USER_DELETE_USER_ADDRESS:
        {
            UserInfoProto::DeleteUserAddressReq deleteUserAddressReq;
            pbToObj(pkg.vecmsgdata(i), deleteUserAddressReq);
            iRet = onDeleteUserAddress(pkg, deleteUserAddressReq, sCurServrantAddr, current);
            break;
        }
        case XGameProto::ActionName::USER_AUTH_SAFEPWD:
        {
            UserInfoProto::AuthSafePwdReq authSafePwdReq;
            pbToObj(pkg.vecmsgdata(i), authSafePwdReq);
            iRet = onAuthUserSafePwd(pkg, authSafePwdReq, sCurServrantAddr, current);
            break;
        }
        case XGameProto::ActionName::USER_LIST_USER_PROPS:
        {
            UserInfoProto::ListUserPropsReq listUserPropsReq;
            pbToObj(pkg.vecmsgdata(i), listUserPropsReq);
            iRet = onListUserProps(pkg, listUserPropsReq, sCurServrantAddr, current);
            break;
        }
        case XGameProto::ActionName::USER_UPDATE_USER_REMAKE:
        {
            UserInfoProto::UpdateUserRemarkReq updateUserRemarkReq;
            pbToObj(pkg.vecmsgdata(i), updateUserRemarkReq);
            iRet = onUpdateUserRemark(pkg, updateUserRemarkReq, sCurServrantAddr, current);
            break;
        }
        case XGameProto::ActionName::USER_SHOW_PROP_INFO:
        {
            UserInfoProto::ShowPropInfoReq showPropInfoReq;
            pbToObj(pkg.vecmsgdata(i), showPropInfoReq);
            iRet = onShowPropInfo(pkg, showPropInfoReq, sCurServrantAddr, current);
            break;
        }
        case XGameProto::ActionName::USER_MARK_PROP_INFO:
        {
            UserInfoProto::MarkPropInfoReq markPropInfoReq;
            pbToObj(pkg.vecmsgdata(i), markPropInfoReq);
            iRet = onMarkProInfo(pkg, markPropInfoReq, sCurServrantAddr, current);
            break;
        }
        case XGameProto::ActionName::USER_LIST_USER_REMARK:
        {
            iRet = onListUserRemark(pkg, sCurServrantAddr, current);
            break;
        }
        default:
        {
            ROLLLOG_ERROR << "invalid msg id, uid: " << lUin << ", msg id: " << head.nmsgid() << endl;
            break;
        }
        }

        if (iRet != 0)
        {
            ROLLLOG_ERROR << "Message process fail: uid : " << lUin << ", msgid: " << head.nmsgid() << ", iRet: " << iRet << endl;
        }
        else
        {
            ROLLLOG_DEBUG << "Message process succ: uid : " << lUin << ", msgid: " << head.nmsgid() << ", iRet: " << iRet << endl;
        }

        int64_t ms2 = TNOWMS;
        if ((ms2 - ms1) > COST_MS)
        {
            ROLLLOG_WARN << "@Performance, msgid:" << head.nmsgid() << ", costTime:" << (ms2 - ms1) << endl;
        }
    }

    __CATCH__

    return iRet;
}

using namespace std;
// static void decodeRecord(const string &data, task::TaskRecord &record);

#define __NASH_RELEASE__
#ifdef __NASH_RELEASE__
static const bool DEBUG_VERSION = false;
#else
static const bool DEBUG_VERSION = true;
#endif // __NASH_RELEASE__

#define __CATCH_OUT_OF_RANGE__ }\
    catch (std::out_of_range& e)\
    {\
            ostringstream os;   \
            os << "[OUT_OF_RANGE]:"<<__FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ ;   \
            throw logic_error(os.str());    \
    }

//获取用户基本信息
tars::Int32 HallServantImp::getUserBasic(const userinfo::GetUserBasicReq &req, userinfo::GetUserBasicResp &resp, tars::TarsCurrentPtr current)
{
    long consumStartMs = TNOWMS;
    int iRet = 0;
    __TRY__

    iRet = UserInfoProcessorSingleton::getInstance()->selectUserInfo(req.uid, resp.userinfo);
    resp.resultCode = iRet;

    __CATCH__
    FUNC_COST_MS(consumStartMs);
    return iRet;
}

//获取用户账户
tars::Int32 HallServantImp::getUserAccount(const userinfo::GetUserAccountReq &req, userinfo::GetUserAccountResp &resp, tars::TarsCurrentPtr current)
{
    long consumStartMs = TNOWMS;
    int iRet = 0;
    __TRY__

    iRet = UserInfoProcessorSingleton::getInstance()->selectUserAccount(req.uid, resp.useraccount);
    resp.resultCode = iRet;

    __CATCH__
    FUNC_COST_MS(consumStartMs);
    return iRet;
}

// 更新用户信息
tars::Int32 HallServantImp::UpdateUserInfo(const userinfo::UpdateUserInfoReq &req, userinfo::UpdateUserInfoResp &resp, tars::TarsCurrentPtr current)
{
    long consumStartMs = TNOWMS;
    int iRet = 0;
    __TRY__

    iRet = UserInfoProcessorSingleton::getInstance()->updateUserInfo(req.uid, req.updateInfo, resp.userinfo);
    resp.resultCode = iRet;
    __CATCH__
    FUNC_COST_MS(consumStartMs);
    return iRet;
}

//添加用户
tars::Int32 HallServantImp::createUser(const userinfo::InitUserReq &req, userinfo::InitUserResp &resp, tars::TarsCurrentPtr current)
{
    long consumStartMs = TNOWMS;
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    iRet = UserInfoProcessorSingleton::getInstance()->initUser(req, resp);

    __CATCH__
    FUNC_EXIT("", iRet);
    FUNC_COST_MS(consumStartMs);
    return iRet;
}

//修改用户账户
tars::Int32 HallServantImp::updateUserAccount(const userinfo::UpdateUserAccountReq &req, userinfo::UpdateUserAccountResp &resp, tars::TarsCurrentPtr current)
{
    long consumStartMs = TNOWMS;
    int iRet = 0;
    __TRY__

    iRet = UserInfoProcessorSingleton::getInstance()->updateUserAccount(req.uid, req.updateInfo, resp.useraccount);
    resp.resultCode = iRet;
    __CATCH__
    FUNC_COST_MS(consumStartMs);
    return iRet;
}

//更新道具(背包道具, 账户道具)
tars::Int32 HallServantImp::modifyUserProps(const userinfo::ModifyUserPropsReq &req, userinfo::ModifyUserPropsResp &resp, tars::TarsCurrentPtr current)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    map<long, push::PropsChangeNotify> mapNotify;
    for(auto modifyInfo : req.updateInfo)
    {
        long curCount = 0;
        long uid = modifyInfo.uid;
        string uuid = modifyInfo.propsInfo.uuid;
        int propsID = modifyInfo.propsInfo.propsID;
        int propsCount = modifyInfo.propsCount;
        LOG_DEBUG << "uid:"<< uid << ", propsID:"<< propsID<<", propsCount:"<< propsCount<<", uuid:"<< uuid << endl;

        config::E_Props_Type propsType = g_app.getOuterFactoryPtr()->getPropsTypeById(propsID);
        switch(propsType)
        {
            case config::E_Props_Type::E_PROPS_TYPE_ACCOUNT:
            {
                string colName = g_app.getOuterFactoryPtr()->getPropsColById(propsID);
                if(colName.empty())
                {
                    LOG_ERROR << "get props colName err. iRet:"<< iRet << endl;
                    break;
                }

                iRet = UserInfoProcessorSingleton::getInstance()->updateUserWealth(uid, {{colName, propsCount},}, &curCount);
                if(iRet != 0)
                {
                    LOG_ERROR << "update mealth err. iRet:"<< iRet << endl;
                }
                break;
            }
            default:
            {
                if(modifyInfo.propsInfo.iState == 0)//新增
                {
                    for(int i = 0; i < propsCount; ++i)
                    {
                        modifyInfo.propsInfo.uuid = ServiceUtil::generateUUIDStr();
                        modifyInfo.propsInfo.propsType = propsType;
                        modifyInfo.propsInfo.getTime = TNOW;
                        iRet = UserInfoProcessorSingleton::getInstance()->insertUserProps(uid, modifyInfo.propsInfo);
                        if(iRet != 0)
                        {
                            LOG_DEBUG << "create props err. "<< endl;
                        }
                    }
                }
                else
                {
                    propsCount = std::abs(propsCount);
                    curCount = UserInfoProcessorSingleton::getInstance()->getPropsCountById(uid, propsID);
                    map<string, string> updateInfo = {
                        {"state", I2S(modifyInfo.propsInfo.iState)},
                    };
                    if(modifyInfo.propsInfo.iState == 1)
                    {
                        updateInfo.insert(std::make_pair("cost_time", ServiceUtil::GetTimeFormat(TNOW)));
                    }
                    if(uuid.empty())
                    {
                        iRet = UserInfoProcessorSingleton::getInstance()->updateUserPropsByID(uid, propsID, propsCount, updateInfo);
                        if(iRet != 0)
                        {
                            LOG_ERROR << "update props err. iRet:"<< iRet << endl;
                            continue;
                        }
                        curCount -= propsCount;
                    }
                    else
                    {
                        for(int i = 0; i < propsCount; ++i)
                        {
                            iRet = UserInfoProcessorSingleton::getInstance()->updateUserPropsByUUID(uid, uuid, updateInfo);
                            if(iRet != 0)
                            {
                                LOG_ERROR << "update props err. iRet:"<< iRet << endl;
                                continue;
                            }
                            curCount--;
                        }
                    }
                }
                break;
            }
        }

        //推送道具变更
        push::PropsInfo info;
        info.iPropsID = modifyInfo.propsInfo.propsID;
        info.iPropsCount = curCount;
        info.iChangeType = modifyInfo.changeType;
        auto itNotify = mapNotify.find(modifyInfo.uid);
        if(itNotify != mapNotify.end())
        {
            itNotify->second.vecPropsInfo.push_back(info);
        }
        else
        {
            push::PropsChangeNotify notify;
            notify.vecPropsInfo.push_back(info);
            mapNotify.insert(std::make_pair(modifyInfo.uid, notify));
        }


        //流水记录
        DaqiGame::TLog2DBReq tLog2DBReq;
        tLog2DBReq.sLogType = 28;

        vector<string> veclog;
        veclog.push_back(L2S(modifyInfo.uid));
        veclog.push_back(I2S(modifyInfo.propsInfo.propsID));
        veclog.push_back(L2S(modifyInfo.changeType));
        veclog.push_back(L2S(modifyInfo.propsCount));
        veclog.push_back(L2S(curCount));
        veclog.push_back(modifyInfo.paraExt);
        veclog.push_back("0");
        tLog2DBReq.vecLogData.push_back(veclog);
        g_app.getOuterFactoryPtr()->asyncLog2DB(modifyInfo.uid, tLog2DBReq);

    }
    g_app.getOuterFactoryPtr()->asyncPushPropsInfo(mapNotify);

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

tars::Int32 HallServantImp::getUserPropsById(long lUin, int propsId, tars::TarsCurrentPtr current)
{
    FUNC_ENTRY("");
    int count = 0;

    config::E_Props_Type propsType = g_app.getOuterFactoryPtr()->getPropsTypeById(propsId);
    LOG_DEBUG << "lUin:"<< lUin << ", propsId:"<< propsId << ", propsType:"<< propsType << endl;

    switch(propsType)
    {
        case config::E_Props_Type::E_PROPS_TYPE_ACCOUNT:
        {
            string colName = g_app.getOuterFactoryPtr()->getPropsColById(propsId);
            if(colName.empty())
            {
                LOG_ERROR << "get props colName err. lUin:"<< lUin << endl;
                break;
            }
            count = UserInfoProcessorSingleton::getInstance()->selectUserWealth(lUin, colName);
            break;
        }
        default:
        {
            count = UserInfoProcessorSingleton::getInstance()->getPropsCountById(lUin, propsId);
            break;
        }
    }
    FUNC_EXIT("", 0);
    return count;
}

//创建房间
tars::Int32 HallServantImp::createMatchRoom(const room::createMatchRoomReq &req, room::createMatchRoomResp &resp, tars::TarsCurrentPtr current)
{
    long consumStartMs = TNOWMS;
    int iRet = 0;
    __TRY__

    iRet = RoomProcessorSingleton::getInstance()->createMatchRoom(req, resp);
    resp.resultCode = iRet;
    __CATCH__
    FUNC_COST_MS(consumStartMs);
    return iRet;
}

//更新房间
tars::Int32 HallServantImp::updateMatchRoom(const room::updateMatchRoomReq &req, room::updateMatchRoomResp &resp, tars::TarsCurrentPtr current)
{
    long consumStartMs = TNOWMS;
    int iRet = 0;
    __TRY__

    iRet = RoomProcessorSingleton::getInstance()->updateMatchRoom(req, resp);
    resp.resultCode = iRet;
    __CATCH__
    FUNC_COST_MS(consumStartMs);
    return iRet;
}

//更新房间成员
tars::Int32 HallServantImp::updateMatchRoomMember(const room::updateRoomMemberReq &req, room::updateRoomMemberResp &resp, tars::TarsCurrentPtr current)
{
    long consumStartMs = TNOWMS;
    int iRet = 0;
    __TRY__

    iRet = RoomProcessorSingleton::getInstance()->updateMatchRoomMember(req, resp);
    __CATCH__
    FUNC_COST_MS(consumStartMs);
    return iRet;
}

tars::Int32 HallServantImp::deleteMatchRoomMember(const string &sRoomIndex, tars::TarsCurrentPtr current)
{
    long consumStartMs = TNOWMS;
    int iRet = 0;
    __TRY__

    iRet = RoomProcessorSingleton::getInstance()->deleteMatchRoomMember(sRoomIndex);
    __CATCH__
    FUNC_COST_MS(consumStartMs);
    return iRet;
}

//获取房间列表
tars::Int32 HallServantImp::selectMatchRoomList(const room::selectMatchRoomListReq &req, room::selectMatchRoomListResp &resp, tars::TarsCurrentPtr current)
{
    long consumStartMs = TNOWMS;
    int iRet = 0;
    __TRY__

    vector<room::MatchRoomInfo> vecRoomInfo;
    vector<room::MatchRoomInfo> vecAllRoomInfo;

    vector<vector<string>> whlist = {
        {"online_game", "0" ,I2S(req.onlineGame)},//=
    };

    if(req.vecGameStatus.size()> 0)
    {
        string inWhil = "(";
        for(auto it = req.vecGameStatus.begin(); it != req.vecGameStatus.end(); it++)
        {
            inWhil += I2S(*it) + (std::next(it) ==req.vecGameStatus.end() ? "" : ",");
        }
        inWhil += ")";
        whlist.push_back({"game_status", "7" , inWhil});
    }

    iRet = RoomProcessorSingleton::getInstance()->selectMatchRoomList(whlist, vecAllRoomInfo);
    if(iRet != 0)
    {
        LOG_ERROR << "select match room list err. iRet: "<< iRet << endl;
        return iRet;
    }

    for(auto &itemInfo : vecAllRoomInfo)
    {
        if(HallProto::E_MTT_STATUS(itemInfo.gameStatus) == HallProto::E_MTT_STATUS::E_MTT_END)
        {
            continue;
        }
        //查询成员
        vector<room::MemberInfo> vecMemberInfo;
        iRet = RoomProcessorSingleton::getInstance()->selectMatchRoomMember(itemInfo.roomIndex, vecMemberInfo);
        if(iRet != 0)
        {
            LOG_ERROR << "select member info err. iRet: "<< iRet << endl;
            continue;
        }
        itemInfo.vecMemberInfo.insert(itemInfo.vecMemberInfo.begin(), vecMemberInfo.begin(), vecMemberInfo.end());
        vecRoomInfo.push_back(itemInfo);
    }

    resp.resultCode = iRet;
    resp.vecMatchRoomInfo.insert(resp.vecMatchRoomInfo.begin(), vecRoomInfo.begin(), vecRoomInfo.end());
    __CATCH__
    FUNC_COST_MS(consumStartMs);
    return iRet;
}

//更新玩家经验
tars::Int32 HallServantImp::updateUserExp(long lUid, config::E_EXP_CHANGE_TYPE eChangeType, tars::TarsCurrentPtr current)
{
    long consumStartMs = TNOWMS;
    int iRet = 0;
    __TRY__

    userinfo::UserInfo userinfo;
    iRet = UserInfoProcessorSingleton::getInstance()->selectUserInfo(lUid, userinfo);
    if (iRet != 0)
    {
        ROLLLOG_ERROR << "getUserBasic failed, lUid: " << lUid << ", iRet: "<< iRet << endl;
        return iRet;
    }

    long totalExp = userinfo.experience + g_app.getOuterFactoryPtr()->getExpChangeByType(eChangeType);
    userinfo.experience = totalExp;

    for(;;)
    {
        int expUpg = g_app.getOuterFactoryPtr()->getExpUpgByLevel(userinfo.level);
        if((totalExp -= expUpg) < 0)
        {
            break;
        }
        if(userinfo.level >= g_app.getOuterFactoryPtr()->getExpMaxLevel())
        {
            userinfo.experience = expUpg;
            userinfo.level = g_app.getOuterFactoryPtr()->getExpMaxLevel();
            break;
        }
        userinfo.experience -= expUpg;
        userinfo.level++;
    }

    map<string, string> updateInfo = {
        {"level", I2S(userinfo.level)},
        {"experience", I2S(userinfo.experience)}
    };

    iRet = UserInfoProcessorSingleton::getInstance()->updateUserInfo(lUid, updateInfo, userinfo);
    if(iRet != 0)
    {
        LOG_ERROR << "update userinfo err, lUid: " << lUid << ", iRet: "<< iRet << endl;
    }

    __CATCH__
    FUNC_COST_MS(consumStartMs);
    return iRet;
}

int HallServantImp::getUserBasic(long uid, UserInfoProto::GetUserBasicResp &getUserBasicResp)
{
    int iRet = 0;
    userinfo::UserInfo userinfo;
    iRet = UserInfoProcessorSingleton::getInstance()->selectUserInfo(uid, userinfo);
    if (iRet != 0)
    {
        ROLLLOG_ERROR << "getUserBasic failed, uid: " << uid << endl;
        return iRet;
    }
    getUserBasicResp.set_gender(userinfo.gender);
    getUserBasicResp.set_name(userinfo.nickname);
    getUserBasicResp.set_head(userinfo.head_str);
    getUserBasicResp.set_signature(userinfo.signature);
    getUserBasicResp.set_rewardpoint(userinfo.reward_point);
    getUserBasicResp.set_entrypoint(userinfo.entry_point);
    getUserBasicResp.set_videopoint(userinfo.video_point);
    getUserBasicResp.set_safepwd((userinfo.safe_pwd.empty() ? userinfo.safe_pwd : "1"));
    getUserBasicResp.set_telephone(userinfo.telephone);

    getUserBasicResp.mutable_userexpinfo()->set_ilevel(userinfo.level);
    getUserBasicResp.mutable_userexpinfo()->set_iexperience(userinfo.experience);
    getUserBasicResp.mutable_userexpinfo()->set_icurlevelexp(g_app.getOuterFactoryPtr()->getExpUpgByLevel(userinfo.level));

    userinfo::UserAccount useraccount;
    iRet = UserInfoProcessorSingleton::getInstance()->selectUserAccount(uid, useraccount);
    if (iRet != 0)
    {
        ROLLLOG_ERROR << "selectUserAccount failed, uid: " <<uid << endl;
        return -2;
    }

    if(useraccount.idc_verify == 1)
    {
        getUserBasicResp.set_name(useraccount.realname);
        getUserBasicResp.set_gender(ServiceUtil::GetGendeByIDC(useraccount.idc_no));
    }

    getUserBasicResp.set_idcverify(useraccount.idc_verify);
    getUserBasicResp.set_areacode(useraccount.area_id);
    getUserBasicResp.set_username(useraccount.username);
    getUserBasicResp.set_systemtime(TNOW);
    getUserBasicResp.set_regtime(ServiceUtil::GetTimeStamp(useraccount.reg_time));

    LOG_DEBUG << "getUserBasicResp: "<< logPb(getUserBasicResp) << endl;

    return 0;
}

//获取用户基本信息
int HallServantImp::onGetUserBasic(const XGameComm::TPackage &pkg, const UserInfoProto::GetUserBasicReq &getUserBasicReq, const std::string &sCurServrantAddr, tars::TarsCurrentPtr current)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__
    ROLLLOG_DEBUG << "getUserBasicReq:" << logPb(getUserBasicReq) << ", luid:" << pkg.stuid().luid() << endl;

    //应答消息
    UserInfoProto::GetUserBasicResp getUserBasicResp;
    iRet = getUserBasic(pkg.stuid().luid(), getUserBasicResp);
    if(iRet != 0)
    {
        ROLLLOG_ERROR << "get userinfo err. iRet: "<< iRet << endl;
    }
    getUserBasicResp.set_resultcode(iRet);
    toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::USER_GET_USER_BASIC, XGameComm::MSGTYPE_RESPONSE, XGameComm::SERVICE_TYPE::SERVICE_TYPE_HALL, getUserBasicResp);

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

//批量获取用户基本信息
int HallServantImp::onListUserBasic(const XGameComm::TPackage &pkg, const UserInfoProto::ListUserBasicReq &listUserBasicReq, bool getfriend, const std::string &sCurServrantAddr, tars::TarsCurrentPtr current)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    LOG_DEBUG << "listUserBasicReq:"<< logPb(listUserBasicReq)<< endl;


    UserInfoProto::ListUserBasicResp listUserBasicResp;

    for (int i = 0; i < listUserBasicReq.uidlist_size(); ++i)
    {
        UserInfoProto::GetUserBasicResp getUserBasicResp;
        if (listUserBasicReq.uidlist(i) >= 100000000)
        {
            //非法的uid
            ROLLLOG_DEBUG << "uid is invalid, uidlist(i).uid: " << listUserBasicReq.uidlist(i) << endl;
            continue;
        }

        getUserBasicResp.set_sremarkcontent(UserInfoProcessorSingleton::getInstance()->selectUserRemark(pkg.stuid().luid(), listUserBasicReq.uidlist(i)));

        getUserBasicResp.set_suserstat(g_app.getOuterFactoryPtr()->getGameRecordServantPrx(pkg.stuid().luid())->getUserStat(listUserBasicReq.uidlist(i)));

        iRet = getUserBasic(listUserBasicReq.uidlist(i), getUserBasicResp);
        if(iRet != 0)
        {
            ROLLLOG_ERROR << "get userinfo err. iRet: "<< iRet << endl;
        }
        else
        {
            (*listUserBasicResp.mutable_basiclist())[listUserBasicReq.uidlist(i)] = getUserBasicResp;
        }
    }

    listUserBasicResp.set_resultcode(0);//客户端要求不传错误码， 不能理解
    LOG_DEBUG<<"listUserBasicResp: "<< logPb(listUserBasicResp)<< endl;
    toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::USER_LIST_USER_BASIC, XGameComm::MSGTYPE_RESPONSE, XGameComm::SERVICE_TYPE::SERVICE_TYPE_HALL, listUserBasicResp);

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

// 更新用户信息
int HallServantImp::onUpdateUserInfo(const XGameComm::TPackage &pkg, const UserInfoProto::UpdateUserInfoReq &updateUserInfoReq, const JFGame::UserBaseInfoExt &stUserBaseInfo, const std::string &sCurServrantAddr)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    LOG_DEBUG << "onUpdateUserInfo:"<< logPb(updateUserInfoReq)<< endl;

    UserInfoProto::UpdateUserInfoResp updateUserInfoResp;

    map<string, string> mStringToString = {
        {"gender", "gender"},
        {"name", "nickname"},
        {"head", "head_str"},
        {"telephone", "telephone"},
        {"signature", "signature"}
    };

    userinfo::UserInfo tUserInfo;
    map<string, string> updateInfo;
    for(auto item : updateUserInfoReq.updateinfo())
    {
        auto iter = mStringToString.find(item.colname());
        if(iter == mStringToString.end())
        {
            iRet = -1;
            goto END;
        }
        updateInfo.insert(std::make_pair(iter->second, item.colvalue()));

        auto ptr = updateUserInfoResp.add_updateinfo();
        ptr->set_colname(item.colname());
        ptr->set_colvalue(item.colvalue());
    }

    //check value
    for(auto info : updateInfo)
    {
        if(info.first == "nickname" || info.first == "signature")
        {
            string content = info.second;
            if(g_app.getOuterFactoryPtr()->checkStrUtf8(content))
            {
                iRet = info.first == "nickname" ? XGameRetCode::USER_INFO_NICKNAME_SPECIAL_CHARACTER: XGameRetCode::USER_INFO_SING_SPECIAL_CHARACTER;
                goto END;
            }
            if((info.first == "signature" && ServiceUtil::getWordCount(content) > MAX_SIGNATURE_LEN) ||
                (info.first == "nickname" && ServiceUtil::getWordCount(content) > MAX_NICKNAME_LEN))
            {
                iRet = XGameRetCode::USER_INFO_REMARK_LENGTH_LIMIT;
                goto END;
            }
        }
        else if(info.first == "gender")
        {
            if(info.second != "1" && info.second != "2")
            {
                iRet = XGameRetCode::USER_INFO_NO_GENDER;
                goto END;
            }
        }
    }

    iRet = UserInfoProcessorSingleton::getInstance()->updateUserInfo(pkg.stuid().luid(), updateInfo, tUserInfo);
    if(iRet != 0)
    {
        LOG_ERROR << "update userinfo err. iRet:"<< iRet << endl;
    }

END:
    updateUserInfoResp.set_resultcode(iRet);
    toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::USER_UPDATE_USER_INFO, XGameComm::MSGTYPE_RESPONSE, XGameComm::SERVICE_TYPE::SERVICE_TYPE_HALL, updateUserInfoResp);
    
    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int HallServantImp::matchRoomList(const long lPlayerID, const HallProto::MatchRoomListReq &req, HallProto::MatchRoomListResp &resp)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    if(req.ipage() <= 0)
    {
        LOG_ERROR <<"req param err."<< endl;
        return -1;
    }

    vector<room::MatchRoomInfo> vecRoomInfo;
    if(req.bselfroom())
    {
       iRet = RoomProcessorSingleton::getInstance()->selectSelfRoomList(lPlayerID, vecRoomInfo);
        if(iRet != 0)
        {
            LOG_ERROR << "select match room list err. iRet: "<< iRet << endl;
            return iRet;
        }
    }
    else
    {
        vector<vector<string>> whlist = {
            {"online_game", "0" ,I2S(req.onlinegame())},//=
        };

        vector<room::MatchRoomInfo> vecAllRoomInfo;
        iRet = RoomProcessorSingleton::getInstance()->selectMatchRoomList(whlist, vecAllRoomInfo);
        if(iRet != 0)
        {
            LOG_ERROR << "select match room list err. iRet: "<< iRet << endl;
            return iRet;
        }

        for(auto itemInfo : vecAllRoomInfo)
        {
            auto it = resp.maproomstatus().find(itemInfo.gameStatus);
            if(it == resp.maproomstatus().end())
            {
                (*resp.mutable_maproomstatus())[itemInfo.gameStatus] = 1;
            }
            else
            {
                (*resp.mutable_maproomstatus())[itemInfo.gameStatus] += 1;
            }

            string beginDate;
            string endDate;
            if(req.onlinegame() == 1)
            {
                int iOffsetDay = req.ioffsetday();
                if(iOffsetDay < 0 || iOffsetDay > 2)//最近三天
                {
                    beginDate = ServiceUtil::GetTimeFormat((TNOW / 86400) * 86400 - 8 * 3600);
                    endDate = ServiceUtil::GetTimeFormat(ServiceUtil::GetTimeStamp(beginDate) + 86400 * 3);
                }
                else
                {
                    beginDate = ServiceUtil::GetTimeFormat((TNOW / 86400) * 86400 - 8 * 3600 + iOffsetDay * 86400);;
                    endDate = ServiceUtil::GetTimeFormat(ServiceUtil::GetTimeStamp(beginDate) + 86400);
                }
                LOG_DEBUG << "beginDate:"<< beginDate<< ", endDate:"<< endDate << endl;

                string matchBeginTime = ServiceUtil::GetTimeFormat(itemInfo.beginTime);
                if(matchBeginTime < beginDate || matchBeginTime >= endDate)
                {
                    continue;
                }
            }

            if(req.vecgamestatus_size() > 0 && req.vecgamestatus().end() == std::find(req.vecgamestatus().begin(), req.vecgamestatus().end(), itemInfo.gameStatus))
            {
                continue;
            }
            vecRoomInfo.push_back(itemInfo);
        }
    }
    LOG_DEBUG<< "vecRoomInfo size:"<< vecRoomInfo.size() << endl;

    unsigned int startIndex = (req.ipage() - 1) * PAGE_COUNT;
    for(unsigned int index = startIndex; index < vecRoomInfo.size(); index++)
    {
        if(resp.roominfos_size() >= PAGE_COUNT)
        {
            break;
        }

        auto info = resp.add_roominfos();
        info->set_roomindex(vecRoomInfo[index].roomIndex);
        info->set_icon(vecRoomInfo[index].icon);
        info->set_roomkey(vecRoomInfo[index].roomKey);
        info->set_roomid(vecRoomInfo[index].roomId);
        info->set_roomname(vecRoomInfo[index].roomName);
        info->set_begintime(vecRoomInfo[index].beginTime);
        info->set_gametype(vecRoomInfo[index].gameType);
        info->set_matchtype(vecRoomInfo[index].matchType);
        info->set_tabletype(vecRoomInfo[index].tableType);
        info->set_onlinegame(vecRoomInfo[index].onlineGame);
        info->set_seatnum(vecRoomInfo[index].seatCount);
        info->set_bdelaysignup(vecRoomInfo[index].bDelaySignUp);
        info->set_brebuy(vecRoomInfo[index].bRebuy);
        info->set_baddon(vecRoomInfo[index].bAddon);
        info->set_curplayercount(vecRoomInfo[index].curPlayerCount);
        info->set_maxplayercount(vecRoomInfo[index].maxPlayerCount);
        info->set_beginsignuptime(vecRoomInfo[index].beginSignUpTime);
        info->set_endsignuptime(vecRoomInfo[index].endSignUpTime);
        info->set_gamestatus(HallProto::E_MTT_STATUS(vecRoomInfo[index].gameStatus));
        info->set_itag(vecRoomInfo[index].iTag);
        info->set_createtime(vecRoomInfo[index].createTime);

        map<string, string> mapRewardInfo;
        g_app.getOuterFactoryPtr()->parseProps(vecRoomInfo[index].rewardInfo, mapRewardInfo);
        for(auto item : mapRewardInfo)
        {
            auto reward = info->add_rewardinfo();
            reward->set_propsid(S2I(item.first));
            reward->set_propscount(S2L(item.second));

            config::PropsInfoCfg infoCfg;
            iRet =  g_app.getOuterFactoryPtr()->getPropsInfoConfig(S2I(item.first), infoCfg);
            if (iRet == 0)
            {
                reward->set_spropsiconsmall(infoCfg.propsIconSmall);
                reward->set_spropsiconbig(infoCfg.propsIconBig);
                reward->set_spropsname(infoCfg.propsName);
            }
        }
    }

    resp.set_icurrpage(req.ipage());
    resp.set_ipagecount((vecRoomInfo.size() % PAGE_COUNT ? 1 : 0) + vecRoomInfo.size() / PAGE_COUNT);

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int HallServantImp::onMatchRoomList(const XGameComm::TPackage &pkg, const HallProto::MatchRoomListReq &req, const std::string &sCurServrantAddr, tars::TarsCurrentPtr current)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    HallProto::MatchRoomListResp resp;
    iRet = matchRoomList(pkg.stuid().luid(), req, resp);
    if(iRet != 0)
    {
        LOG_ERROR << "query match list err. iRet:"<< iRet << endl;
    }

    resp.set_iresultid(iRet);
    LOG_DEBUG <<"resp: "<< logPb(resp)<< endl;
    toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::HALL_MATCH_ROOM_LIST, XGameComm::MSGTYPE_RESPONSE, XGameComm::SERVICE_TYPE::SERVICE_TYPE_HALL, resp);

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

// 获取用户地址信息
int HallServantImp::onListUserAddress(const XGameComm::TPackage &pkg, const std::string &sCurServrantAddr, tars::TarsCurrentPtr current)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    long lUid = pkg.stuid().luid();

    LOG_DEBUG << "onListUserAddress:" << lUid << endl;


    UserInfoProto::ListUserAddressResp listUserAddressResp;

    vector<UserAddress> vAddress;
    iRet = UserInfoProcessorSingleton::getInstance()->selectUserAddress(lUid, vAddress);
    if(iRet != 0)
    {
        ROLLLOG_ERROR << "onListUserAddress err. iRet: "<< iRet << endl;
    }
    else
    {
        for (auto item : vAddress)
        {
            auto pAddress = listUserAddressResp.add_infos();
            pAddress->set_gid(item.gid);
            pAddress->set_uid(item.uid);
            pAddress->set_status(item.status);
            pAddress->set_nickname(item.nickname);
            pAddress->set_telephone(item.telephone);
            pAddress->set_address(item.address);
        }
    }
    
    listUserAddressResp.set_resultcode(iRet);
    LOG_DEBUG<<"listUserAddressResp: "<< logPb(listUserAddressResp)<< endl;
    toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::USER_LIST_USER_ADDRESS, XGameComm::MSGTYPE_RESPONSE, XGameComm::SERVICE_TYPE::SERVICE_TYPE_HALL, listUserAddressResp);

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int HallServantImp::onAddUserAddress(const XGameComm::TPackage &pkg, const UserInfoProto::AddUserAddressReq &addUserAddressReq, const std::string &sCurServrantAddr, tars::TarsCurrentPtr current)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    long lUid = pkg.stuid().luid();

    LOG_DEBUG << "onAddUserAddress:" << logPb(addUserAddressReq) << endl;

    TGetTableGUIDReq insertIDReq;
    insertIDReq.keyIndex = 0;
    insertIDReq.tableName = "tb_user_address_gid";
    insertIDReq.fieldName = "gid";

    TGetTableGUIDRsp insertIDRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(lUid)->getTableGUID(insertIDReq, insertIDRsp);
    ROLLLOG_DEBUG << "get last insert id, iRet: " << iRet << ", insertIDRsp: " << printTars(insertIDRsp) << endl;
    if (insertIDRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "fetch new user id err, iRet: " << iRet << ", iResult: " << insertIDRsp.iResult << endl;
        return XGameRetCode::LOGIN_SERVER_ERROR;
    }

    long lGid = insertIDRsp.lastID;

    map<string, string> updateAddress;
    updateAddress.insert(std::make_pair("gid", L2S(lGid)));
    updateAddress.insert(std::make_pair("uid", L2S(lUid)));
    updateAddress.insert(std::make_pair("address", addUserAddressReq.address()));
    updateAddress.insert(std::make_pair("telephone", addUserAddressReq.telephone()));
    updateAddress.insert(std::make_pair("nickname", addUserAddressReq.nickname()));
    updateAddress.insert(std::make_pair("status", I2S(addUserAddressReq.status())));

    // 如果是默认的，先清一下原来所有数据的status
    if (addUserAddressReq.status() == 1)
    {
        iRet = UserInfoProcessorSingleton::getInstance()->resetUserAddress(lUid);
        if (iRet != 0)
        {
            ROLLLOG_ERROR << "resetUserAddress err. iRet: "<< iRet << endl;
        }
    }

    iRet = UserInfoProcessorSingleton::getInstance()->updateUserAddress(lGid, updateAddress, true);

    if(iRet != 0)
    {
        ROLLLOG_ERROR << "onAddUserAddress err. iRet: "<< iRet << endl;
    }

    UserInfoProto::AddUserAddressResp addUserAddressResp;

    if (iRet == 0)
    {
        auto pInfo = addUserAddressResp.mutable_info();
        pInfo->set_gid(lGid);
        pInfo->set_uid(lUid);
        pInfo->set_status(addUserAddressReq.status());
        pInfo->set_nickname(addUserAddressReq.nickname());
        pInfo->set_telephone(addUserAddressReq.telephone());
        pInfo->set_address(addUserAddressReq.address());
    }

    addUserAddressResp.set_resultcode(iRet);
    toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::USER_ADD_USER_ADDRESS, XGameComm::MSGTYPE_RESPONSE, XGameComm::SERVICE_TYPE::SERVICE_TYPE_HALL, addUserAddressResp);

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int HallServantImp::onUpdateUserAddress(const XGameComm::TPackage &pkg, const UserInfoProto::UpdateUserAddressReq &updateUserAddressReq, const std::string &sCurServrantAddr, tars::TarsCurrentPtr current)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    LOG_DEBUG << "onUpdateUserAddress:" << logPb(updateUserAddressReq) << endl;

    map<string, string> mStringToString = {
        {"status", "status"},
        {"nickname", "nickname"},
        {"telephone", "telephone"},
        {"address", "address"}
    };

    long lUid = pkg.stuid().luid();
    long lGid = updateUserAddressReq.gid();

    bool bStatus = false;
    map<string, string> updateAddress;
    for (int i = 0; i < updateUserAddressReq.updatedata_size(); ++i)
    {
        auto iter = mStringToString.find(updateUserAddressReq.updatedata(i).colname());
        if (iter != mStringToString.end())
        {
            updateAddress.insert(std::make_pair(iter->second, updateUserAddressReq.updatedata(i).colvalue()));
            if (updateUserAddressReq.updatedata(i).colname() == "status" && updateUserAddressReq.updatedata(i).colvalue() == "1")
            {
                bStatus = true;
            }
        }
    }

    // 如果是默认的，先清一下原来所有数据的status
    if (bStatus)
    {
        iRet = UserInfoProcessorSingleton::getInstance()->resetUserAddress(lUid);
        if (iRet != 0)
        {
            ROLLLOG_ERROR << "resetUserAddress err. iRet: "<< iRet << endl;
        }
    }

    iRet = UserInfoProcessorSingleton::getInstance()->updateUserAddress(lGid, updateAddress, false);

    if(iRet != 0)
    {
        ROLLLOG_ERROR << "onUpdateUserAddress err. iRet: "<< iRet << endl;
    }

    UserInfoProto::UpdateUserAddressResp updateUserAddressResp;
    if (iRet == 0)
    {
        for (auto item : updateAddress)
        {
            auto pData = updateUserAddressResp.add_updatedata();
            pData->set_colname(item.first);
            pData->set_colvalue(item.second);
        }
    }
    updateUserAddressResp.set_resultcode(iRet);
    updateUserAddressResp.set_gid(lGid);
    toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::USER_UPDATE_USER_ADDRESS, XGameComm::MSGTYPE_RESPONSE, XGameComm::SERVICE_TYPE::SERVICE_TYPE_HALL, updateUserAddressResp);

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int HallServantImp::onDeleteUserAddress(const XGameComm::TPackage &pkg, const UserInfoProto::DeleteUserAddressReq &deleteUserAddressReq, const std::string &sCurServrantAddr, tars::TarsCurrentPtr current)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    LOG_DEBUG << "onDeleteUserAddress:" << logPb(deleteUserAddressReq) << endl;

    long lGid = deleteUserAddressReq.gid();

    iRet = UserInfoProcessorSingleton::getInstance()->deleteUserAddress(lGid);

    if(iRet != 0)
    {
        ROLLLOG_ERROR << "onDeleteUserAddress err. iRet: "<< iRet << endl;
    }

    UserInfoProto::DeleteUserAddressResp deleteUserAddressResp;
    deleteUserAddressResp.set_resultcode(iRet);
    deleteUserAddressResp.set_gid(lGid);
    toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::USER_DELETE_USER_ADDRESS, XGameComm::MSGTYPE_RESPONSE, XGameComm::SERVICE_TYPE::SERVICE_TYPE_HALL, deleteUserAddressResp);

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int HallServantImp::onAuthUserSafePwd(const XGameComm::TPackage &pkg, const UserInfoProto::AuthSafePwdReq &req, const std::string &sCurServrantAddr, tars::TarsCurrentPtr current)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__
    userinfo::UserInfo userinfo;
    iRet = UserInfoProcessorSingleton::getInstance()->selectUserInfo(pkg.stuid().luid(), userinfo);
    if (iRet != 0)
    {
        ROLLLOG_ERROR << "getUserBasic failed, lUin: " << pkg.stuid().luid() << endl;
        goto END;
    }
    if(userinfo.safe_pwd != req.ssafepwd())
    {
        iRet = XGameRetCode::USER_INFO_SAFEPWD_NOT_EQUAL;
        ROLLLOG_ERROR << "safe pwd not equal. req safe pwd: " << req.ssafepwd() << ", user safe pwd:"<< userinfo.safe_pwd << endl;
        goto END;
    }

END:
    UserInfoProto::AuthSafePwdResp rsp;
    rsp.set_resultcode(iRet);
    toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::USER_AUTH_SAFEPWD, XGameComm::MSGTYPE_RESPONSE, XGameComm::SERVICE_TYPE::SERVICE_TYPE_HALL, rsp);

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int HallServantImp::onListUserProps(const XGameComm::TPackage &pkg, const UserInfoProto::ListUserPropsReq &req, const std::string &sCurServrantAddr, tars::TarsCurrentPtr current)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    long lUid = pkg.stuid().luid();

    LOG_DEBUG << "onListUserProps:" << lUid << endl;


    UserInfoProto::ListUserPropsResp listUserPropsResp;

    vector<userinfo::PropsInfo> vecPropsInfo;
    iRet = UserInfoProcessorSingleton::getInstance()->selectUserProps(lUid, vecPropsInfo);
    if(iRet != 0)
    {
        ROLLLOG_ERROR << "selectUserProps err. iRet: "<< iRet << endl;
    }
    else
    {
        config::PropsInfoCfg infoCfg;
        for (auto item : vecPropsInfo)
        {
            if (req.itype() != 0 && req.itype() != item.propsType)
            {
                continue;
            }

            iRet =  g_app.getOuterFactoryPtr()->getPropsInfoConfig(item.propsID, infoCfg);
            if (iRet != 0)
            {
                LOG_ERROR << "getPropsInfoConfig config not find id:" << item.propsID << endl;
                continue;
            }

            auto pProps = listUserPropsResp.add_infos();
            pProps->set_uid(lUid);
            pProps->set_pid(item.propsID);
            pProps->set_ptype(item.propsType);
            pProps->set_istate(item.iState);
            pProps->set_gtime(item.getTime);
            pProps->set_ctime(item.costTime);
            pProps->set_suuid(item.uuid);
            pProps->set_schannel(item.getChannel);
            pProps->set_saddress(infoCfg.propsDesc);
            pProps->set_sname(infoCfg.propsName);
            pProps->set_siconbig(infoCfg.propsIconBig);
            pProps->set_siconsmall(infoCfg.propsIconSmall);
        }
    }
    
    listUserPropsResp.set_resultcode(0);
    listUserPropsResp.set_itype(req.itype());
    LOG_DEBUG<<"listUserPropsResp: "<< logPb(listUserPropsResp)<< endl;
    toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::USER_LIST_USER_PROPS, XGameComm::MSGTYPE_RESPONSE, XGameComm::SERVICE_TYPE::SERVICE_TYPE_HALL, listUserPropsResp);

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int HallServantImp::onUpdateUserRemark(const XGameComm::TPackage &pkg, const UserInfoProto::UpdateUserRemarkReq &req, const std::string &sCurServrantAddr, tars::TarsCurrentPtr current)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    UserInfoProto::UpdateUserRemarkResp resp;

    iRet = UserInfoProcessorSingleton::getInstance()->updateUserRemark(pkg.stuid().luid(), req.remarkuid(), req.sremarkcontent());
    if(iRet != 0)
    {
        LOG_ERROR << "update user remark err. req:"<< logPb(req)<< endl;
    }

    resp.set_resultcode(iRet);
    toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::USER_UPDATE_USER_REMAKE, XGameComm::MSGTYPE_RESPONSE, XGameComm::SERVICE_TYPE::SERVICE_TYPE_HALL, resp);

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int HallServantImp::onShowPropInfo(const XGameComm::TPackage &pkg, const UserInfoProto::ShowPropInfoReq &req, const std::string &sCurServrantAddr, tars::TarsCurrentPtr current)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    long lUid = pkg.stuid().luid();
    UserInfoProto::ShowPropInfoResp resp;

    userinfo::PropsInfo propsItem;
    iRet = UserInfoProcessorSingleton::getInstance()->selectUserPropsByUUID(req.luid(), req.scode(), propsItem);
    if(iRet != 0)
    {
        LOG_ERROR << "props not exist. iRet:"<< iRet << ", uuid:"<< req.scode() << ", uid:"<< req.luid() << endl;
        resp.set_resultcode(XGameRetCode::USER_INFO_NO_PROP);
        toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::USER_SHOW_PROP_INFO, XGameComm::MSGTYPE_RESPONSE, XGameComm::SERVICE_TYPE::SERVICE_TYPE_HALL, resp);
        return -1;
    }

    resp.set_resultcode(iRet);
    resp.set_scode(req.scode());
    resp.set_splatform(propsItem.getChannel);
    resp.set_lgettime(propsItem.getTime);
    resp.set_lendtime(propsItem.costTime);
    resp.set_istatus(propsItem.iState);
    config::PropsInfoCfg infoCfg;
    iRet =  g_app.getOuterFactoryPtr()->getPropsInfoConfig(propsItem.propsID, infoCfg);
    if (iRet == 0)
    {
        resp.set_sname(infoCfg.propsName);
        resp.set_saddress(infoCfg.propsDesc);
    }

    if (req.luid() == lUid)
    {
        UserInfo userinfo;
        iRet = UserInfoProcessorSingleton::getInstance()->selectUserInfo(req.luid(), userinfo);
        if (iRet == 0)
        {
            UserAccount useraccount;
            iRet = UserInfoProcessorSingleton::getInstance()->selectUserAccount(req.luid(), useraccount);
            if (iRet == 0)
            {
                resp.set_snickname(userinfo.nickname);
                resp.set_susercode(useraccount.idc_no);
                if (userinfo.telephone != "")
                {
                    resp.set_stelephone(userinfo.telephone);
                }
                else
                {
                    resp.set_stelephone(useraccount.username);
                }
            }
        }
    }
    toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::USER_SHOW_PROP_INFO, XGameComm::MSGTYPE_RESPONSE, XGameComm::SERVICE_TYPE::SERVICE_TYPE_HALL, resp);

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int HallServantImp::onMarkProInfo(const XGameComm::TPackage &pkg, const UserInfoProto::MarkPropInfoReq &req, const std::string &sCurServrantAddr, tars::TarsCurrentPtr current)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    UserInfoProto::MarkPropInfoResp resp;

    string sUuid = req.scode();

    userinfo::PropsInfo propsItem;
    iRet = UserInfoProcessorSingleton::getInstance()->selectUserPropsByUUID(req.luid(), sUuid, propsItem);
    if(iRet != 0)
    {
        LOG_ERROR << "props not exist. iRet:"<< iRet << ", uuid:"<< sUuid << ", uid:"<< req.luid() << endl;
        resp.set_resultcode(XGameRetCode::USER_INFO_NO_PROP);
        toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::USER_MARK_PROP_INFO, XGameComm::MSGTYPE_RESPONSE, XGameComm::SERVICE_TYPE::SERVICE_TYPE_HALL, resp);
        return -1;
    }

    if (propsItem.iState != 0)
    {
        LOG_ERROR << "props is marked uuid:"<< sUuid << ", state:"<< propsItem.iState << endl;
        resp.set_resultcode(XGameRetCode::USER_INFO_PROP_MARK);
        toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::USER_MARK_PROP_INFO, XGameComm::MSGTYPE_RESPONSE, XGameComm::SERVICE_TYPE::SERVICE_TYPE_HALL, resp);
        return -2;
    }

    long lNow = TNOW;
    if (propsItem.costTime < lNow)
    {
        LOG_ERROR << "props is marked uuid:"<< sUuid << ", costTime:"<< propsItem.costTime << ", now:" << lNow << endl;
        resp.set_resultcode(XGameRetCode::USER_INFO_PROP_INFO_TIME_LIMIT);
        toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::USER_MARK_PROP_INFO, XGameComm::MSGTYPE_RESPONSE, XGameComm::SERVICE_TYPE::SERVICE_TYPE_HALL, resp);
        return -3;
    }

    map<string, string> updateInfo = {
        {"state", "1"},
    };

    iRet = UserInfoProcessorSingleton::getInstance()->updateUserPropsByUUID(req.luid(), sUuid, updateInfo);
    if (iRet != 0)
    {
        resp.set_resultcode(XGameRetCode::USER_INFO_PROP_MARK_ERROR);
    }
    else
    {
        resp.set_resultcode(iRet);
    }
    
    toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::USER_MARK_PROP_INFO, XGameComm::MSGTYPE_RESPONSE, XGameComm::SERVICE_TYPE::SERVICE_TYPE_HALL, resp);

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

int HallServantImp::onListUserRemark(const XGameComm::TPackage &pkg, const std::string &sCurServrantAddr, tars::TarsCurrentPtr current)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    long lUid = pkg.stuid().luid();

    LOG_DEBUG << "onListUserMark:" << lUid << endl;


    UserInfoProto::ListUserRemarkResp listUserRemarkResp;

    vector<userinfo::UserRemarkInfo> vecRemarkInfo;
    iRet = UserInfoProcessorSingleton::getInstance()->listUserRemark(lUid, vecRemarkInfo);
    if(iRet != 0)
    {
        ROLLLOG_ERROR << "selectUserRemark err. iRet: "<< iRet << endl;
    }
    else
    {
        userinfo::UserInfo userinfo;
        for (auto item : vecRemarkInfo)
        {
            if (item.content == "")
            {
                continue;
            }

            iRet = UserInfoProcessorSingleton::getInstance()->selectUserInfo(item.uid, userinfo);
            if (iRet != 0)
            {
                continue;
            }

            auto pRemark = listUserRemarkResp.add_infos();
            pRemark->set_uid(item.uid);
            pRemark->set_head(userinfo.head_str);
            pRemark->set_name(userinfo.nickname);
            pRemark->set_content(item.content);
        }
    }
    
    listUserRemarkResp.set_resultcode(0);
    LOG_DEBUG<<"listUserRemarkResp: "<< logPb(listUserRemarkResp)<< endl;
    toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::USER_LIST_USER_REMARK, XGameComm::MSGTYPE_RESPONSE, XGameComm::SERVICE_TYPE::SERVICE_TYPE_HALL, listUserRemarkResp);

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

//发送消息到客户端
template<typename T>
int HallServantImp::toClientPb(const XGameComm::TPackage &tPackage, const std::string &sCurServrantAddr, XGameProto::ActionName actionName, XGameComm::MSGTYPE type, int serviceType, const T &t)
{
    XGameComm::TPackage rsp;
    rsp.set_iversion(tPackage.iversion());

    auto ptuid = rsp.mutable_stuid();
    ptuid->set_luid(tPackage.stuid().luid());
    rsp.set_igameid(tPackage.igameid());
    rsp.set_sroomid(tPackage.sroomid());
    rsp.set_iroomserverid(tPackage.iroomserverid());
    rsp.set_isequence(tPackage.isequence());
    rsp.set_iflag(tPackage.iflag());

    auto mh = rsp.add_vecmsghead();
    mh->set_nmsgid(actionName);
    mh->set_nmsgtype(XGameComm::MSGTYPE::MSGTYPE_RESPONSE);
    mh->set_servicetype((XGameComm::SERVICE_TYPE)serviceType);
    rsp.add_vecmsgdata(pbToString(t));

    auto pPushPrx = Application::getCommunicator()->stringToProxy<JFGame::PushPrx>(sCurServrantAddr);
    if (pPushPrx)
    {
        ROLLLOG_DEBUG << "toClientPb, uid: " << tPackage.stuid().luid() << ", actionName: " << actionName
                      << ", toclient pb: " << logPb(rsp) << ", t: " << logPb(t) << endl;
        pPushPrx->tars_hash(tPackage.stuid().luid())->async_doPushBuf(NULL, tPackage.stuid().luid(), pbToString(rsp));
    }
    else
    {
        ROLLLOG_ERROR << "pPushPrx is null, uid: " << tPackage.stuid().luid() << ", actionName: " << actionName
                      << ", toclient pb: " << logPb(rsp) << ", t: " << logPb(t) << endl;
    }
    return 0;
}

template<typename T>
int HallServantImp::toClientPb(const XGameComm::TPackage &tPackage, const std::string &sCurServrantAddr, XGameProto::ActionName actionName, int serviceType, const T &t)
{
    XGameComm::TPackage rsp;
    rsp.set_iversion(tPackage.iversion());

    auto ptuid = rsp.mutable_stuid();
    ptuid->set_luid(tPackage.stuid().luid());
    rsp.set_igameid(tPackage.igameid());
    rsp.set_sroomid(tPackage.sroomid());
    rsp.set_iroomserverid(tPackage.iroomserverid());
    rsp.set_isequence(tPackage.isequence());
    rsp.set_iflag(tPackage.iflag());

    auto mh = rsp.add_vecmsghead();
    mh->set_nmsgid(actionName);
    mh->set_nmsgtype(XGameComm::MSGTYPE::MSGTYPE_RESPONSE);
    mh->set_servicetype((XGameComm::SERVICE_TYPE)serviceType);
    rsp.add_vecmsgdata(pbToString(t));

    auto pPushPrx = Application::getCommunicator()->stringToProxy<JFGame::PushPrx>(sCurServrantAddr);
    if (pPushPrx)
    {
        ROLLLOG_DEBUG << "toClientPb, uid: " << tPackage.stuid().luid() << ", actionName: " << actionName
                      << ", toclient pb: " << logPb(rsp) << ", t: " << logPb(t) << endl;
        pPushPrx->tars_hash(tPackage.stuid().luid())->async_doPushBuf(NULL, tPackage.stuid().luid(), pbToString(rsp));
    }
    else
    {
        ROLLLOG_ERROR << "pPushPrx is null, uid: " << tPackage.stuid().luid() << ", actionName: " << actionName
                      << ", toclient pb: " << logPb(rsp) << ", t: " << logPb(t) << endl;
    }

    return 0;
}

template<typename T>
int HallServantImp::toClientPb(const XGameComm::TPackage &tPackage, const std::string &sCurServrantAddr, XGameProto::ActionName actionName, const T &t, int serviceType, XGameComm::MSGTYPE msgType)
{
    XGameComm::TPackage rsp;
    //rsp.set_iversion(tPackage.iversion());


    auto mh = rsp.add_vecmsghead();
    mh->set_nmsgid(actionName);
    mh->set_nmsgtype(msgType);
    mh->set_servicetype((XGameComm::SERVICE_TYPE)serviceType);
    rsp.add_vecmsgdata(pbToString(t));

    auto pPushPrx = Application::getCommunicator()->stringToProxy<JFGame::PushPrx>(sCurServrantAddr);
    if (pPushPrx)
    {
        //ROLLLOG_DEBUG << "toClientPb, uid: " << tPackage.stuid().luid() << ", actionName: " << actionName  << ", toclient pb: " << logPb(rsp) << ", t: " << logPb(t) << endl;
        pPushPrx->tars_hash(tPackage.stuid().luid())->async_doPushBuf(NULL, tPackage.stuid().luid(), pbToString(rsp));
    }
    else
    {
        ROLLLOG_ERROR << "pPushPrx is null, uid: " << tPackage.stuid().luid() << ", actionName: " << actionName  << ", toclient pb: " << logPb(rsp) << ", t: " << logPb(t) << endl;
    }

    return 0;
}

tars::Int32 HallServantImp::doCustomMessage(bool bExpectIdle)
{
    return 0;
}

