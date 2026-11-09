#include "tungsten/game/clientgame/presentable/presentablevehiclecar.h"

#include <algorithm>
#include <cmath>
#include <cstring>

// DIA: idPresentableVehicle::ShouldEnableSphereCollision
bool idPresentableVehicle::ShouldEnableSphereCollision() const {
  return sphereCollisionEnabled;
}

// DIA: LerpAngle
// DIA: idPhysics_Vehicle::~idPhysics_Vehicle
// DIA: `idPhysics_Vehicle::~idPhysics_Vehicle'::`1'::dtor$0
// DIA: idPhysics_Vehicle::UnlinkClip
// DIA: GetEffectsMaterial
// DIA: idEffectPhysicsDebrisEmitter::~idEffectPhysicsDebrisEmitter
// DIA: idPresentableVehicle::Interpolate
// DIA: idPresentableVehicle::SetBoostDuration
// DIA: idPresentableVehicle::GetCurrentWeaponGroup
// DIA: idPresentableVehicle::GetCrosshair
// DIA: idPresentableVehicle::DeactivateCrosshair
// DIA: idPresentableVehicle::WeaponFiredWithNoAmmo
// DIA: idPresentableVehicle::AddProjectileLock
// DIA: idPhysics_Vehicle::`scalar deleting destructor'
// DIA: idPhysics_Vehicle::Evaluate
// DIA: idPhysics_Vehicle::DisableClip
// DIA: idPhysics_Vehicle::EnableClip
// DIA: idPhysics_Vehicle::SetShotClipModel
// DIA: idPhysics_Vehicle::UpdateVehicleClipModels
// DIA: idPresentableVehiclePhysicsCB::GetName
// DIA: idPresentableVehiclePhysicsCB::GetMasterTransform
// DIA: idPresentableVehiclePhysicsCB::Contact
// DIA: idPresentableVehiclePhysicsCB::ApplyImpulse
// DIA: idPresentableVehiclePhysicsCB::ApplyForce
// DIA: idPresentableVehiclePhysicsCB::Crush
// DIA: idPresentableVehiclePhysicsCB::ApplyDamage
// DIA: idPresentableVehiclePhysicsCB::ActivatePhysics
// DIA: idPresentableVehiclePhysicsCB::DeactivatePhysics
// DIA: idPresentableVehiclePhysicsCB::ApplyWaterEffects
// DIA: idPresentableVehiclePhysicsCB::ApplyWaterSplashEffects
// DIA: idPresentableVehicle::Present
// DIA: idPresentableVehicle::InitDebrisEmitter
// DIA: idPresentableVehicle::InitImpactMgr
// DIA: GetSurfaceSound
// DIA: idPresentableVehicle::KillEngineSounds
// DIA: idPresentableVehicle::InventoryAdded
// DIA: idPresentableVehicle::InitVehicleBody
// DIA: `idPresentableVehicle::InitVehicleBody'::`1'::dtor$0
// DIA: `idPresentableVehicle::InitVehicleBody'::`1'::dtor$1
// DIA: `idPresentableVehicle::InitVehicleBody'::`1'::dtor$2
// DIA: idPresentableVehicle::InitVehiclePhysics
// DIA: idPresentableVehicle::UpdateCurrentMPH
// DIA: idPresentableVehicle::GetBoostPower
// DIA: idPresentableVehicle::GetBoostDuration
// DIA: idPresentableVehicle::ResetBoostPower
// DIA: idPresentableVehicle::BoostForce
// DIA: idPresentableVehicle::RunPhysics
// DIA: Mat3DiffToAngularVelocity
// DIA: idPresentableVehicle::UseSteerControl
// DIA: idPresentableVehicle::PitchRollStabilize
// DIA: idPresentableVehicle::GetViewOriginAndAxis
// DIA: ComputeCameraViewOrigin
// DIA: idPresentableVehicle::GetDriverView
// DIA: idPresentableVehicle::ResetDriverView
// DIA: idPresentableVehicle::AdjustCg
// DIA: idPresentableVehicle::UpdateStickControl
// DIA: idPresentableVehicle::ResetBoostFOV
// DIA: idPresentableVehicle::StopBoost
// DIA: idPresentableVehicle::GetHealth
// DIA: idPresentableVehicle::GetHasInfiniteAmmo
// DIA: idPresentableVehicle::PredictPickup
// DIA: idPresentableVehicle::GetInputSettings
// DIA: idPresentableVehicle::GetPredictedFXMgr
// DIA: idPresentableVehicle::UpdateFX
// DIA: idPresentableVehicle::UpdateFX
// DIA: idPresentableVehicle::InitFXMgr
// DIA: idPresentableVehicle::ShutdownFXMgr
// DIA: idPresentableVehicle::StopPredictedFX
// DIA: idPresentableVehicle::DrawBirdsView
// DIA: idPresentableVehicle::GetTagTransform
// DIA: idPresentableVehicle::ClearDamageFX
// DIA: idPresentableVehicle::TransformDetachable
// DIA: idPresentableVehicle::PredictHitScanHit
// DIA: idPresentableVehicle::RemoveProjectileLock
// DIA: idPresentableAnimatedEntity::SerializeAnimatorPtr<idAnimator_VehicleCar>
// DIA: idListArrayNew<wheelSlipInfo_t>
// DIA: idEffectPhysicsDebrisEmitter::idEffectPhysicsDebrisEmitter
// DIA: idVehicle::GetOccupant
// DIA: idPhysics_Vehicle::Translate
// DIA: idPhysics_Vehicle::Rotate
// DIA: idPhysics_Vehicle::LinkClip
// DIA: idPresentableVehicle::SerializePredictionOnly
// DIA: idPresentableVehicle::SerializeNonPredictionState
// DIA: idPresentableVehicle::HandleWheelDustTrail
// DIA: idPresentableVehicle::HandleWheelDust
// DIA: idPresentableVehicle::CheckRampLanding
// DIA: idPresentableVehicle::DropTireMark
// DIA: idPresentableVehicle::GetWeaponForGroup
// DIA: idPresentableVehicle::GetMaxSpeed
// DIA: idPresentableVehicle::ArcadeSteering
// DIA: idPresentableVehicle::AutoRollOver
// DIA: idPresentableVehicle::CalcWheelsOnGround
// DIA: idPresentableVehicle::UpdateHorn
// DIA: idPresentableVehicle::StopBoostFX
// DIA: idPresentableVehicle::SetBoostDisabled
// DIA: idPresentableVehicle::GetUserCommand
// DIA: idPresentableVehicle::DeleteWeapons
// DIA: idPresentableVehicle::UpdateDetachableItems
// DIA: idPresentableVehicle::WeaponGroupHasAmmo
// DIA: idPresentableVehicle::SetCurrentWeapon
// DIA: idPresentableVehicle::GetWeaponDecl
// DIA: idPresentableVehicle::RecordClientDamageInfo
// DIA: idPresentableVehicle::GetWeaponView
// DIA: idPresentableVehicle::GetDetachableWorldTransform
// DIA: idListArrayResize<wheelSlipInfo_t>
// DIA: idThirdPersonCrosshair::HasOwner
// DIA: idThirdPersonCrosshair::GetTarget
// DIA: idPresentableVehicle::SerializePredictionState
// DIA: idPresentableVehicle::PredictSpecialDeath
// DIA: idPresentableVehicle::UpdateWheelSkid
// DIA: idPresentableVehicle::HandleBoostViewFx
// DIA: idPresentableVehicle::GetSimpleCameraInternal
// DIA: idPresentableVehicle::StopBlurFX
// DIA: idPresentableVehicle::PlayerIsOccupant
// DIA: idPresentableVehicle::GetDriver
// DIA: idPresentableVehicle::GetPlayerDriver
// DIA: idPresentableVehicle::GetGunner
// DIA: idPresentableVehicle::GetDriverEntity
// DIA: vehicleOccupant_t::GetVisualAsEntity
// DIA: vehicleOccupant_t::GetNpcAsEntity
// DIA: idPresentableVehicle::GetNumActiveOccupants
// DIA: idPresentableVehicle::IsLocallyControlled
// DIA: idPresentableVehicle::IsLocallyControlledByPlayer
// DIA: idPresentableVehicle::IsRemotelyControlledByPlayer
// DIA: idPresentableVehicle::ClientHandleDamageInfo
// DIA: idPresentableVehicle::GetClientFire
// DIA: idPresentableVehicle::GetPassengerView
// DIA: idPresentableVehicle::GetFPSDriverView
// DIA: idPresentableVehicle::UpdateWhiplash
// DIA: idPresentableVehicle::ThrowAttachedItem
// DIA: idPresentableVehicle::TriggerBoostExtender
// DIA: idPresentableVehicle::GetLockedTarget
// DIA: idList<idWheel,5>::Resize
// DIA: idVehicleWeapon::GetPresentable
// DIA: collisionInfo_t::collisionInfo_t
// DIA: idList<vehicleOccupant_t,5>::Resize
// DIA: idPresentableVehicle::PostSerializeRead
// DIA: idPresentableVehicle::ServerOverridingPosition
// DIA: idPresentableVehicle::UpdateTireTracks
// DIA: idPresentableVehicle::GetFXMgrForActiveWeapon
// DIA: idPresentableVehicle::UpdateRemoteClientSimulation
// DIA: idPresentableVehicle::GetDriverViewInternalTest
// DIA: idPresentableVehicle::UpdateDriverViewAngles
// DIA: idPresentableVehicle::UpdateExhaust
// DIA: idPresentableVehicle::StartSoundShader_Predicted
// DIA: idPresentableVehicle::StopSound_Predicted
// DIA: idPresentableVehicle::GetPeerIndex
// DIA: idPresentableVehicle::SetCurrentWeaponGroup
// DIA: `idPresentableVehicle::SetCurrentWeaponGroup'::`1'::dtor$0
// DIA: idPresentableVehicle::Draw
// DIA: idPresentableVehicle::ThrowAllDetchableItems
// DIA: idPresentableVehicle::ActivateWeaponGroup
// DIA: idPresentableVehicle::StartMannedWeaponChange
// DIA: idPresentableVehicle::ActivateCrosshair
// DIA: idPresentableVehicle::IsTargetLockable
// DIA: idPresentableVehicle::UpdateCrosshair
// DIA: idPresentableVehicle::ClientHitScanHit
// DIA: idPresentableVehicle::ClientHitScanHit_ClientFire
// DIA: idPresentableVehicle::RecordClientFire
// DIA: `idPresentableVehicle::RecordClientFire'::`1'::dtor$0
// DIA: idPresentableVehicle::EvaluateClientWeaponSwitch
// DIA: idList<idPresentableVehicle::clientDetachable_t,5>::~idList<idPresentableVehicle::clientDetachable_t,5>
// DIA: idList<idPresentableVehicle::clientDetachable_t,5>::Resize
// DIA: idStaticList<idPresentableVehicle::clientDetachable_t,70>::~idStaticList<idPresentableVehicle::clientDetachable_t,70>
// DIA: `idStaticList<idPresentableVehicle::clientDetachable_t,70>::~idStaticList<idPresentableVehicle::clientDetachable_t,70>'::`1'::dtor$0
// DIA: idPresentableVehicle::~idPresentableVehicle
// DIA: `idPresentableVehicle::~idPresentableVehicle'::`1'::dtor$0
// DIA: `idPresentableVehicle::~idPresentableVehicle'::`1'::dtor$1
// DIA: `idPresentableVehicle::~idPresentableVehicle'::`1'::dtor$2
// DIA: `idPresentableVehicle::~idPresentableVehicle'::`1'::dtor$3
// DIA: `idPresentableVehicle::~idPresentableVehicle'::`1'::dtor$4
// DIA: `idPresentableVehicle::~idPresentableVehicle'::`1'::dtor$5
// DIA: `idPresentableVehicle::~idPresentableVehicle'::`1'::dtor$6
// DIA: `idPresentableVehicle::~idPresentableVehicle'::`1'::dtor$7
// DIA: `idPresentableVehicle::~idPresentableVehicle'::`1'::dtor$8
// DIA: `idPresentableVehicle::~idPresentableVehicle'::`1'::dtor$9
// DIA: `idPresentableVehicle::~idPresentableVehicle'::`1'::dtor$10
// DIA: `idPresentableVehicle::~idPresentableVehicle'::`1'::dtor$11
// DIA: `idPresentableVehicle::~idPresentableVehicle'::`1'::dtor$14
// DIA: idPresentableVehicle::GetImpactMgrPtr
// DIA: idPresentableVehicle::GetInventory
// DIA: idPresentableVehicle::GetControllingPlayerIndex
// DIA: idPresentableVehicle::GetControllingPlayer
// DIA: idPresentableVehicle::Init
// DIA: `idPresentableVehicle::Init'::`1'::dtor$0
// DIA: `idPresentableVehicle::Init'::`1'::dtor$1
// DIA: idPresentableVehicle::SerializeWeaponList
// DIA: `idPresentableVehicle::SerializeWeaponList'::`1'::dtor$0
// DIA: idPresentableVehicle::SerializeOccupants
// DIA: idPresentableVehicle::UpdateRemoteSimulationParms_Server
// DIA: idPresentableVehicle::UpdateAnimator
// DIA: idPresentableVehicle::Respawn
// DIA: idPresentableVehicle::UpdateDriverView
// DIA: idPresentableVehicle::GetSoundDef
// DIA: `idPresentableVehicle::GetSoundDef'::`1'::dtor$0
// DIA: idPresentableVehicle::AddWeaponToVehicle
// DIA: idPresentableVehicle::NextWeapon
// DIA: idPresentableVehicle::SerializeWeaponChange
// DIA: idPresentableVehicle::UpdateWeapons
// DIA: `idPresentableVehicle::UpdateWeapons'::`1'::dtor$0
// DIA: idPresentableVehicle::CheckImpulse_Local
// DIA: idPresentableVehicle::EvaluateClientFire
// DIA: idPresentableVehicle::ThrowDebris
// DIA: idList<idPresentableVehicle::clientDetachable_t,5>::Append
// DIA: idPresentableVehicle::`scalar deleting destructor'
// DIA: idPresentableVehicle::SetDefaults
// DIA: idPresentableVehicle::SerializeDetachableItems
// DIA: idPresentableVehicle::UpdateSounds
// DIA: idPresentableVehicle::UpdateEffects
// DIA: idPresentableVehicle::UpdateSteerAngle
// DIA: idPresentableVehicle::DriveControl_Local
// DIA: idPresentableVehicle::DriveControl
// DIA: idPresentableVehicle::HandleCollisionClientServer
// DIA: idPresentableVehicle::AddDetachableItem
// DIA: idPresentableVehicle::idPresentableVehicle
// DIA: `idPresentableVehicle::idPresentableVehicle'::`1'::dtor$0
// DIA: `idPresentableVehicle::idPresentableVehicle'::`1'::dtor$12
// DIA: `idPresentableVehicle::idPresentableVehicle'::`1'::dtor$1
// DIA: `idPresentableVehicle::idPresentableVehicle'::`1'::dtor$2
// DIA: `idPresentableVehicle::idPresentableVehicle'::`1'::dtor$3
// DIA: `idPresentableVehicle::idPresentableVehicle'::`1'::dtor$4
// DIA: `idPresentableVehicle::idPresentableVehicle'::`1'::dtor$5
// DIA: `idPresentableVehicle::idPresentableVehicle'::`1'::dtor$6
// DIA: `idPresentableVehicle::idPresentableVehicle'::`1'::dtor$7
// DIA: `idPresentableVehicle::idPresentableVehicle'::`1'::dtor$8
// DIA: `idPresentableVehicle::idPresentableVehicle'::`1'::dtor$9
// DIA: `idPresentableVehicle::idPresentableVehicle'::`1'::dtor$10
// DIA: idPresentableVehicle::Serialize
// DIA: idPresentableVehicle::ClientPredict
// DIA: idPresentableVehicle::ServerClientThink
// DIA: idPresentableVehicle::Collide
// DIA: idPresentableVehiclePhysicsCB::Collide
// DIA: idPresentableVehicle::ClientThink
// DIA: idPresentableVehicle::ServerThink
// DIA: `dynamic initializer for 'v_simpleCameraMode''
// DIA: `dynamic initializer for 'v_dusttime''
// DIA: `dynamic initializer for 'v_clientArmor_debug''
// DIA: `dynamic initializer for 'v_maxTurnSpeed''
// DIA: `dynamic initializer for 'v_compressiontime''
// DIA: `dynamic initializer for 'vehicle_violent_roll''
// DIA: `dynamic initializer for 'vehicle_violent_boundsscale''
// DIA: `dynamic initializer for 'vehicle_rcLerp''
// DIA: `dynamic initializer for 'veh_maxRemoteSimulateSpeed''
// DIA: `dynamic initializer for 'veh_maxRemoteSimulateAngSpeed''
// DIA: `dynamic initializer for 'veh_maxSpeedMP''
// DIA: `dynamic initializer for 'veh_maxAngSpeedMP''
// DIA: `dynamic initializer for 'veh_clientOverrideTime''
// DIA: `dynamic initializer for 'veh_useReliableHitMsgs''
// DIA: `dynamic initializer for 'veh_mpSkidRumbleHighScalar''
// DIA: `dynamic initializer for 'veh_mpSkidRumbleLowScalar''
// DIA: `dynamic initializer for 'veh_mpSkidRumbleHighDur''
// DIA: `dynamic initializer for 'veh_mpSkidRumbleLowDur''
// DIA: `dynamic initializer for 'veh_remoteSimulationThresholdWithNoInfluence''
// DIA: `dynamic initializer for 'veh_remoteSimulationThreshold''
// DIA: `dynamic initializer for 'veh_remoteSimulationAngThresholdWithNoInfluence''
// DIA: `dynamic initializer for 'veh_remoteSimulationAngThreshold''
// DIA: `dynamic initializer for 'v_testRolloverRoll''
// DIA: `dynamic initializer for 'v_testRolloverPitch''
// DIA: `dynamic initializer for 'vehicle_birdsView''
// DIA: `dynamic initializer for 'vehicle_birdsViewDistance''
// DIA: `dynamic initializer for 'vehicle_passengerFOV''
// DIA: `dynamic initializer for 'vehicle_fps_view_lerp_time''
// DIA: `dynamic initializer for 'vehicle_driverview_test''
// DIA: `dynamic initializer for 'vehicle_fps_stickAngle_scaleAgainstMax''
// DIA: `dynamic initializer for 'vehicle_fps_stickAngle_minPitch''
// DIA: `dynamic initializer for 'vehicle_fps_stickAngle_maxPitch''
// DIA: `dynamic initializer for 'vehicle_fps_stickAngle_minYaw''
// DIA: `dynamic initializer for 'vehicle_fps_stickAngle_maxYaw''
// DIA: `dynamic initializer for 'vc_wildyaw''
// DIA: `dynamic initializer for 'vc_wildpitch''
// DIA: `dynamic initializer for 'vc_wildroll''
// DIA: `dynamic initializer for 'vc_recoverrate''
// DIA: `dynamic initializer for 'vc_max_pitch''
// DIA: `dynamic initializer for 'v_mouseFreeLook''
// DIA: `dynamic initializer for 'v_boostSlide''
// DIA: `dynamic initializer for 'v_jumpSlide''
// DIA: `dynamic initializer for 'v_jumpActiveTime''
// DIA: `dynamic initializer for 'vehicle_remoteRange''
// DIA: `dynamic initializer for 'v_steerControl''
// DIA: `dynamic initializer for 'v_boostVelocityStabilize''
// DIA: `dynamic initializer for 'v_steerControlBoostRate''
// DIA: `dynamic initializer for 'v_steerControlRate''
// DIA: `dynamic initializer for 'v_steerControlTargetSpeed''
// DIA: `dynamic initializer for 'v_steerControlInfluence''
// DIA: `dynamic initializer for 'v_steerControlClamp''
// DIA: `dynamic initializer for 'v_rollStabilization''
// DIA: `dynamic initializer for 'v_pitchStabilization''
// DIA: `dynamic initializer for 'v_skidRoll''
// DIA: `dynamic initializer for 'v_spinoutAngle''
// DIA: `dynamic initializer for 'v_handbrake_cam_lerp_enable''
// DIA: `dynamic initializer for 'v_handbrake_cam_lerp''
// DIA: `dynamic initializer for 'v_handbrake_cam_lerp_power''
// DIA: `dynamic initializer for 'v_handbrake_cam_lerp_min_mult''
// DIA: `dynamic initializer for 'v_handbrake_skid_lerp_enable''
// DIA: `dynamic initializer for 'v_handbrake_skid_lerp''
// DIA: `dynamic initializer for 'v_handbrake_skid_lerp_power''
// DIA: `dynamic initializer for 'v_handbrake_gripout_lerp_enable''
// DIA: `dynamic initializer for 'v_handbrake_gripout_lerp''
// DIA: `dynamic initializer for 'v_handbrake_gripout_lerp_power''
// DIA: `dynamic initializer for 'v_handbrake_gripin_lerp_enable''
// DIA: `dynamic initializer for 'v_handbrake_gripin_lerp''
// DIA: `dynamic initializer for 'v_handbrake_gripin_lerp_power''
// DIA: `dynamic initializer for 'vehicle_baseFOV''
// DIA: `dynamic initializer for 'vehicle_maxBaseFOV''
// DIA: `dynamic initializer for 'v_mpQuickUseOnSelect''
// DIA: `dynamic initializer for 'veh_eval''
// DIA: `dynamic initializer for 'veh_debug_print''
// DIA: `dynamic initializer for 'veh_volume_brake_whine''
// DIA: `dynamic initializer for 'veh_volume_shift_up''
// DIA: `dynamic initializer for 'veh_volume_shift_down''
// DIA: `dynamic initializer for 'veh_visual_dust_scale_min''
// DIA: `dynamic initializer for 'vehicle_cam_new''
// DIA: `dynamic initializer for 'vehicle_blurOffset''
// DIA: `dynamic initializer for 'vehicle_blurValue''
// DIA: `dynamic initializer for 'vehicle_blurMaxScale''
// DIA: `dynamic initializer for 'vehicle_blurDuration''
// DIA: `dynamic initializer for 'vehicle_blurRampUp''
// DIA: `dynamic initializer for 'vehicle_blurRampDown''
// DIA: `dynamic initializer for 'veh_simulateRemoteVehicles''
// DIA: `dynamic initializer for 'veh_impulseScaleMP''
// DIA: `dynamic initializer for 'veh_hitControlDelay''
// DIA: `dynamic initializer for 'vehicle_swerveForce''
// DIA: `dynamic initializer for 'vehicle_stuckUpForce''
// DIA: `dynamic initializer for 'vehicle_stuckLateralForce''
// DIA: `dynamic initializer for 'vehicle_extra_headon_impulse''
// DIA: `dynamic initializer for 'vehicle_extra_headon_impulse_mult''
// DIA: `dynamic initializer for 'vehicle_extra_headon_impulse_offset''
// DIA: `dynamic initializer for 'vehicle_extra_headon_impulse_mult_rc''
// DIA: `dynamic initializer for 'vehicle_extra_headon_impulse_offset_rc''
// DIA: `dynamic initializer for 'vehicle_extra_headon_impulse_debug''
// DIA: `dynamic initializer for 'vehicle_fx_engineSmokeThreshold''
// DIA: `dynamic initializer for 'vehicle_clientImpactDmg_MinVel''
// DIA: `dynamic initializer for 'vehicle_clientImpactDmg_MaxVel''
// DIA: `dynamic initializer for 'vehicle_clientImpactDmg_MinDmg''
// DIA: `dynamic initializer for 'vehicle_clientImpactDmg_MaxDmg''
// DIA: `dynamic initializer for 'veh_damage_fx_level1_start''
// DIA: `dynamic initializer for 'veh_damage_fx_level2_start''
// DIA: `dynamic initializer for 'veh_damage_fx_level3_start''
// DIA: `dynamic initializer for 'veh_damage_fx_level4_start''
// DIA: `dynamic initializer for 'veh_damage_fx_level1_start_mp''
// DIA: `dynamic initializer for 'veh_damage_fx_level2_start_mp''
// DIA: `dynamic initializer for 'veh_damage_fx_level3_start_mp''
// DIA: `dynamic initializer for 'veh_damage_fx_level4_start_mp''
// DIA: `dynamic initializer for 'vehicle_mp_defOrigin''
// DIA: `dynamic initializer for 'vehicle_health_debug''
// DIA: `dynamic initializer for 'vehicle_soundImpactForce''
// DIA: `dynamic initializer for 'vehicle_online_boostExtenderCooldown''
// DIA: `dynamic initializer for 'vehicle_showWheels''
// DIA: `dynamic initializer for 'vehicle_online_skipServerPhysicsCB''
// DIA: `dynamic initializer for 'vehicle_showExhaust''
// DIA: `dynamic initializer for 'vehicle_useCompressionPitch''
// DIA: `dynamic initializer for 'vehicle_showCompressionPitch''
// DIA: `dynamic initializer for 'vehicle_compressionPitchMin''
// DIA: `dynamic initializer for 'vehicle_compressionPitchMax''
// DIA: `dynamic initializer for 'vehicle_compressionPitchMinScale''
// DIA: `dynamic initializer for 'vehicle_compressionPitchMaxScale''
// DIA: `dynamic initializer for 'vehicle_maxGear''
// DIA: `dynamic initializer for 'vehicle_sound_skipBody''
// DIA: `dynamic initializer for 'vehicle_trailMaxDist''
// DIA: `dynamic initializer for 'g_tireTrackLengthScaleSpeed''
// DIA: `dynamic initializer for 'g_tireTrackLengthScale''
// DIA: `dynamic initializer for 'vehicle_reverseTorqueMultiplier''
// DIA: `dynamic initializer for 'vehicle_emp_mode''
// DIA: `dynamic initializer for 'vehicle_emp_handbrake''
// DIA: `dynamic initializer for 'vehicle_emp_brakeForce''
// DIA: `dynamic initializer for 'vehicle_jumpButton_verticalImpulse''
// DIA: `dynamic initializer for 'v_airControlDelay''
// DIA: `dynamic initializer for 'v_airControlLowerThreshold''
// DIA: `dynamic initializer for 'v_airControlUpperThreshold''
// DIA: `dynamic initializer for 'v_printAirControlDelay''
// DIA: `dynamic initializer for 'weapon_overheatEnable''
// DIA: `dynamic initializer for 'weapon_overheatCoolPercentage''
// DIA: `dynamic initializer for 'weapon_overheatCooldownScale''
// DIA: `dynamic initializer for 'weapon_overheatDebug''
// DIA: `dynamic initializer for 'mp_vehicle_DamageDebug''
// DIA: `dynamic initializer for 'vehicle_wview_lag''
// DIA: `dynamic initializer for 'vehicle_wview_lag_clamp''
// DIA: `dynamic initializer for 'vehicle_wview_alag''
// DIA: `dynamic initializer for 'vehicle_wview_fwd''
// DIA: `dynamic initializer for 'vehicle_wview_rt''
// DIA: `dynamic initializer for 'vehicle_wview_up''
// DIA: `dynamic initializer for 'vehicle_whiplash_min_frac''
// DIA: `dynamic initializer for 'vehicle_whiplash_max_ang''
// DIA: `dynamic initializer for 'vehicle_whiplash_exp''
// DIA: `dynamic initializer for 'vehicle_whiplash_angvel_max_ang''
// DIA: `dynamic initializer for 'vehicle_whiplash_angvel_min_frac''
// DIA: `dynamic initializer for 'vehicle_whiplash_angvel_high''
// DIA: `dynamic initializer for 'vehicle_whiplash_angvel_exp''
// DIA: `dynamic atexit destructor for 'v_simpleCameraMode''
// DIA: `dynamic atexit destructor for 'v_dusttime''
// DIA: `dynamic atexit destructor for 'v_clientArmor_debug''
// DIA: `dynamic atexit destructor for 'v_maxTurnSpeed''
// DIA: `dynamic atexit destructor for 'v_compressiontime''
// DIA: `dynamic atexit destructor for 'vehicle_violent_roll''
// DIA: `dynamic atexit destructor for 'vehicle_violent_boundsscale''
// DIA: `dynamic atexit destructor for 'vehicle_rcLerp''
// DIA: `dynamic atexit destructor for 'veh_maxRemoteSimulateSpeed''
// DIA: `dynamic atexit destructor for 'veh_maxRemoteSimulateAngSpeed''
// DIA: `dynamic atexit destructor for 'veh_maxSpeedMP''
// DIA: `dynamic atexit destructor for 'veh_maxAngSpeedMP''
// DIA: `dynamic atexit destructor for 'veh_clientOverrideTime''
// DIA: `dynamic atexit destructor for 'veh_useReliableHitMsgs''
// DIA: `dynamic atexit destructor for 'veh_mpSkidRumbleHighScalar''
// DIA: `dynamic atexit destructor for 'veh_mpSkidRumbleLowScalar''
// DIA: `dynamic atexit destructor for 'veh_mpSkidRumbleHighDur''
// DIA: `dynamic atexit destructor for 'veh_mpSkidRumbleLowDur''
// DIA: `dynamic atexit destructor for 'veh_remoteSimulationThresholdWithNoInfluence''
// DIA: `dynamic atexit destructor for 'veh_remoteSimulationThreshold''
// DIA: `dynamic atexit destructor for 'veh_remoteSimulationAngThresholdWithNoInfluence''
// DIA: `dynamic atexit destructor for 'veh_remoteSimulationAngThreshold''
// DIA: `dynamic atexit destructor for 'v_testRolloverRoll''
// DIA: `dynamic atexit destructor for 'v_testRolloverPitch''
// DIA: `dynamic atexit destructor for 'vehicle_birdsView''
// DIA: `dynamic atexit destructor for 'vehicle_birdsViewDistance''
// DIA: `dynamic atexit destructor for 'vehicle_passengerFOV''
// DIA: `dynamic atexit destructor for 'vehicle_fps_view_lerp_time''
// DIA: `dynamic atexit destructor for 'vehicle_driverview_test''
// DIA: `dynamic atexit destructor for 'vehicle_fps_stickAngle_scaleAgainstMax''
// DIA: `dynamic atexit destructor for 'vehicle_fps_stickAngle_minPitch''
// DIA: `dynamic atexit destructor for 'vehicle_fps_stickAngle_maxPitch''
// DIA: `dynamic atexit destructor for 'vehicle_fps_stickAngle_minYaw''
// DIA: `dynamic atexit destructor for 'vehicle_fps_stickAngle_maxYaw''
// DIA: `dynamic atexit destructor for 'vc_wildyaw''
// DIA: `dynamic atexit destructor for 'vc_wildpitch''
// DIA: `dynamic atexit destructor for 'vc_wildroll''
// DIA: `dynamic atexit destructor for 'vc_recoverrate''
// DIA: `dynamic atexit destructor for 'vc_max_pitch''
// DIA: `dynamic atexit destructor for 'v_mouseFreeLook''
// DIA: `dynamic atexit destructor for 'v_boostSlide''
// DIA: `dynamic atexit destructor for 'v_jumpSlide''
// DIA: `dynamic atexit destructor for 'v_jumpActiveTime''
// DIA: `dynamic atexit destructor for 'vehicle_remoteRange''
// DIA: `dynamic atexit destructor for 'v_steerControl''
// DIA: `dynamic atexit destructor for 'v_boostVelocityStabilize''
// DIA: `dynamic atexit destructor for 'v_steerControlBoostRate''
// DIA: `dynamic atexit destructor for 'v_steerControlRate''
// DIA: `dynamic atexit destructor for 'v_steerControlTargetSpeed''
// DIA: `dynamic atexit destructor for 'v_steerControlInfluence''
// DIA: `dynamic atexit destructor for 'v_steerControlClamp''
// DIA: `dynamic atexit destructor for 'v_rollStabilization''
// DIA: `dynamic atexit destructor for 'v_pitchStabilization''
// DIA: `dynamic atexit destructor for 'v_skidRoll''
// DIA: `dynamic atexit destructor for 'v_spinoutAngle''
// DIA: `dynamic atexit destructor for 'v_handbrake_cam_lerp_enable''
// DIA: `dynamic atexit destructor for 'v_handbrake_cam_lerp''
// DIA: `dynamic atexit destructor for 'v_handbrake_cam_lerp_power''
// DIA: `dynamic atexit destructor for 'v_handbrake_cam_lerp_min_mult''
// DIA: `dynamic atexit destructor for 'v_handbrake_skid_lerp_enable''
// DIA: `dynamic atexit destructor for 'v_handbrake_skid_lerp''
// DIA: `dynamic atexit destructor for 'v_handbrake_skid_lerp_power''
// DIA: `dynamic atexit destructor for 'v_handbrake_gripout_lerp_enable''
// DIA: `dynamic atexit destructor for 'v_handbrake_gripout_lerp''
// DIA: `dynamic atexit destructor for 'v_handbrake_gripout_lerp_power''
// DIA: `dynamic atexit destructor for 'v_handbrake_gripin_lerp_enable''
// DIA: `dynamic atexit destructor for 'v_handbrake_gripin_lerp''
// DIA: `dynamic atexit destructor for 'v_handbrake_gripin_lerp_power''
// DIA: `dynamic atexit destructor for 'vehicle_baseFOV''
// DIA: `dynamic atexit destructor for 'vehicle_maxBaseFOV''
// DIA: `dynamic atexit destructor for 'v_mpQuickUseOnSelect''
// DIA: `dynamic atexit destructor for 'veh_eval''
// DIA: `dynamic atexit destructor for 'veh_debug_print''
// DIA: `dynamic atexit destructor for 'veh_volume_brake_whine''
// DIA: `dynamic atexit destructor for 'veh_volume_shift_up''
// DIA: `dynamic atexit destructor for 'veh_volume_shift_down''
// DIA: `dynamic atexit destructor for 'veh_visual_dust_scale_min''
// DIA: `dynamic atexit destructor for 'vehicle_cam_new''
// DIA: `dynamic atexit destructor for 'vehicle_blurOffset''
// DIA: `dynamic atexit destructor for 'vehicle_blurValue''
// DIA: `dynamic atexit destructor for 'vehicle_blurMaxScale''
// DIA: `dynamic atexit destructor for 'vehicle_blurDuration''
// DIA: `dynamic atexit destructor for 'vehicle_blurRampUp''
// DIA: `dynamic atexit destructor for 'vehicle_blurRampDown''
// DIA: `dynamic atexit destructor for 'veh_simulateRemoteVehicles''
// DIA: `dynamic atexit destructor for 'veh_impulseScaleMP''
// DIA: `dynamic atexit destructor for 'veh_hitControlDelay''
// DIA: `dynamic atexit destructor for 'vehicle_swerveForce''
// DIA: `dynamic atexit destructor for 'vehicle_stuckUpForce''
// DIA: `dynamic atexit destructor for 'vehicle_stuckLateralForce''
// DIA: `dynamic atexit destructor for 'vehicle_extra_headon_impulse''
// DIA: `dynamic atexit destructor for 'vehicle_extra_headon_impulse_mult''
// DIA: `dynamic atexit destructor for 'vehicle_extra_headon_impulse_offset''
// DIA: `dynamic atexit destructor for 'vehicle_extra_headon_impulse_mult_rc''
// DIA: `dynamic atexit destructor for 'vehicle_extra_headon_impulse_offset_rc''
// DIA: `dynamic atexit destructor for 'vehicle_extra_headon_impulse_debug''
// DIA: `dynamic atexit destructor for 'vehicle_fx_engineSmokeThreshold''
// DIA: `dynamic atexit destructor for 'vehicle_clientImpactDmg_MinVel''
// DIA: `dynamic atexit destructor for 'vehicle_clientImpactDmg_MaxVel''
// DIA: `dynamic atexit destructor for 'vehicle_clientImpactDmg_MinDmg''
// DIA: `dynamic atexit destructor for 'vehicle_clientImpactDmg_MaxDmg''
// DIA: `dynamic atexit destructor for 'veh_damage_fx_level1_start''
// DIA: `dynamic atexit destructor for 'veh_damage_fx_level2_start''
// DIA: `dynamic atexit destructor for 'veh_damage_fx_level3_start''
// DIA: `dynamic atexit destructor for 'veh_damage_fx_level4_start''
// DIA: `dynamic atexit destructor for 'veh_damage_fx_level1_start_mp''
// DIA: `dynamic atexit destructor for 'veh_damage_fx_level2_start_mp''
// DIA: `dynamic atexit destructor for 'veh_damage_fx_level3_start_mp''
// DIA: `dynamic atexit destructor for 'veh_damage_fx_level4_start_mp''
// DIA: `dynamic atexit destructor for 'vehicle_mp_defOrigin''
// DIA: `dynamic atexit destructor for 'vehicle_health_debug''
// DIA: `dynamic atexit destructor for 'vehicle_soundImpactForce''
// DIA: `dynamic atexit destructor for 'vehicle_online_boostExtenderCooldown''
// DIA: `dynamic atexit destructor for 'vehicle_showWheels''
// DIA: `dynamic atexit destructor for 'vehicle_online_skipServerPhysicsCB''
// DIA: `dynamic atexit destructor for 'vehicle_showExhaust''
// DIA: `dynamic atexit destructor for 'vehicle_useCompressionPitch''
// DIA: `dynamic atexit destructor for 'vehicle_showCompressionPitch''
// DIA: `dynamic atexit destructor for 'vehicle_compressionPitchMin''
// DIA: `dynamic atexit destructor for 'vehicle_compressionPitchMax''
// DIA: `dynamic atexit destructor for 'vehicle_compressionPitchMinScale''
// DIA: `dynamic atexit destructor for 'vehicle_compressionPitchMaxScale''
// DIA: `dynamic atexit destructor for 'vehicle_maxGear''
// DIA: `dynamic atexit destructor for 'vehicle_sound_skipBody''
// DIA: `dynamic atexit destructor for 'vehicle_trailMaxDist''
// DIA: `dynamic atexit destructor for 'g_tireTrackLengthScaleSpeed''
// DIA: `dynamic atexit destructor for 'g_tireTrackLengthScale''
// DIA: `dynamic atexit destructor for 'vehicle_reverseTorqueMultiplier''
// DIA: `dynamic atexit destructor for 'vehicle_emp_mode''
// DIA: `dynamic atexit destructor for 'vehicle_emp_handbrake''
// DIA: `dynamic atexit destructor for 'vehicle_emp_brakeForce''
// DIA: `dynamic atexit destructor for 'vehicle_jumpButton_verticalImpulse''
// DIA: `dynamic atexit destructor for 'v_airControlDelay''
// DIA: `dynamic atexit destructor for 'v_airControlLowerThreshold''
// DIA: `dynamic atexit destructor for 'v_airControlUpperThreshold''
// DIA: `dynamic atexit destructor for 'v_printAirControlDelay''
// DIA: `dynamic atexit destructor for 'weapon_overheatEnable''
// DIA: `dynamic atexit destructor for 'weapon_overheatCoolPercentage''
// DIA: `dynamic atexit destructor for 'weapon_overheatCooldownScale''
// DIA: `dynamic atexit destructor for 'weapon_overheatDebug''
// DIA: `dynamic atexit destructor for 'mp_vehicle_DamageDebug''
// DIA: `dynamic atexit destructor for 'vehicle_wview_lag''
// DIA: `dynamic atexit destructor for 'vehicle_wview_lag_clamp''
// DIA: `dynamic atexit destructor for 'vehicle_wview_alag''
// DIA: `dynamic atexit destructor for 'vehicle_wview_fwd''
// DIA: `dynamic atexit destructor for 'vehicle_wview_rt''
// DIA: `dynamic atexit destructor for 'vehicle_wview_up''
// DIA: `dynamic atexit destructor for 'vehicle_whiplash_min_frac''
// DIA: `dynamic atexit destructor for 'vehicle_whiplash_max_ang''
// DIA: `dynamic atexit destructor for 'vehicle_whiplash_exp''
// DIA: `dynamic atexit destructor for 'vehicle_whiplash_angvel_max_ang''
// DIA: `dynamic atexit destructor for 'vehicle_whiplash_angvel_min_frac''
// DIA: `dynamic atexit destructor for 'vehicle_whiplash_angvel_high''
// DIA: `dynamic atexit destructor for 'vehicle_whiplash_angvel_exp''
// DIA: `idPresentableVehicle::GetSoundDef'::`5'::`dynamic atexit destructor for 'fakeSounds''

namespace{idMat3 VehicleIdentity(){idMat3 a{};a[0]={1,0,0};a[1]={0,1,0};a[2]={0,0,1};return a;}}
float LerpAngle(float from,float to,float fraction){float delta=std::fmod(to-from+540.0F,360.0F)-180.0F;return from+delta*std::clamp(fraction,0.0F,1.0F);}const char* GetEffectsMaterial(int surface){static const char* names[]={"default","metal","dirt","water"};return names[std::clamp(surface,0,3)];}const char* GetSurfaceSound(int surface){static const char* sounds[]={"impact/default","impact/metal","impact/dirt","impact/water"};return sounds[std::clamp(surface,0,3)];}
idVec3 Mat3DiffToAngularVelocity(const idMat3& from,const idMat3& to,float dt){if(dt<=0)return{};return{(to[0]-from[0]).Length()/dt,(to[1]-from[1]).Length()/dt,(to[2]-from[2]).Length()/dt};}idVec3 ComputeCameraViewOrigin(const idVec3&o,const idMat3&a,float distance,float height){return o-a[0]*distance+a[2]*height;}
idPhysics_Vehicle::~idPhysics_Vehicle()=default;void idPhysics_Vehicle::UnlinkClip(){clipEnabled=false;}bool idPhysics_Vehicle::Evaluate(float dt){return dt>0&&clipEnabled;}void idPhysics_Vehicle::DisableClip(){clipEnabled=false;}void idPhysics_Vehicle::EnableClip(){clipEnabled=true;}void idPhysics_Vehicle::SetShotClipModel(bool e){shotClip=e;}void idPhysics_Vehicle::UpdateVehicleClipModels(){}void idPhysics_Vehicle::Translate(const idVec3&d){origin+=d;}void idPhysics_Vehicle::Rotate(const idMat3&v){axis=v;}void idPhysics_Vehicle::LinkClip(){clipEnabled=true;}
idEffectPhysicsDebrisEmitter::idEffectPhysicsDebrisEmitter()=default;idEffectPhysicsDebrisEmitter::~idEffectPhysicsDebrisEmitter()=default;
const char* idPresentableVehiclePhysicsCB::GetName()const{return"vehiclePhysics";}bool idPresentableVehiclePhysicsCB::GetMasterTransform(idVec3&o,idMat3&a)const{o={};a=VehicleIdentity();return active;}void idPresentableVehiclePhysicsCB::Contact(float f){damage+=std::max(0.0F,f)*0.01F;}void idPresentableVehiclePhysicsCB::ApplyImpulse(const idVec3&i){damage+=i.Length()*0.001F;}void idPresentableVehiclePhysicsCB::ApplyForce(const idVec3&f){damage+=f.Length()*0.0001F;}void idPresentableVehiclePhysicsCB::Crush(float d){ApplyDamage(d);}void idPresentableVehiclePhysicsCB::ApplyDamage(float d){damage+=std::max(0.0F,d);}void idPresentableVehiclePhysicsCB::ActivatePhysics(){active=true;}void idPresentableVehiclePhysicsCB::DeactivatePhysics(){active=false;}void idPresentableVehiclePhysicsCB::ApplyWaterEffects(){damage=std::max(0.0F,damage-0.1F);}void idPresentableVehiclePhysicsCB::ApplyWaterSplashEffects(){ApplyWaterEffects();}
vehicleOccupant_t* idVehicle::GetOccupant(int i){return i>=0&&i<static_cast<int>(occupants.size())?&occupants[static_cast<std::size_t>(i)]:nullptr;}idEntity* vehicleOccupant_t::GetVisualAsEntity()const{return visual;}idEntity* vehicleOccupant_t::GetNpcAsEntity()const{return npc;}collisionInfo_t::collisionInfo_t()=default;bool idThirdPersonCrosshair::HasOwner()const{return owner!=nullptr;}idEntity* idThirdPersonCrosshair::GetTarget()const{return target;}

idPresentableVehicle::idPresentableVehicle(){SetDefaults();}idPresentableVehicle::~idPresentableVehicle()=default;void idPresentableVehicle::Interpolate(float fraction){currentMPH=LerpAngle(currentMPH,currentMPH,std::clamp(fraction,0.0F,1.0F));}void idPresentableVehicle::SetBoostDuration(float s){boostDuration=std::max(0.0F,s);}int idPresentableVehicle::GetCurrentWeaponGroup()const{return currentWeaponGroup;}bool idPresentableVehicle::GetCrosshair()const{return crosshairActive;}void idPresentableVehicle::DeactivateCrosshair(){crosshairActive=false;crosshair.SetTarget(nullptr);}void idPresentableVehicle::WeaponFiredWithNoAmmo(){++noAmmoCount;}void idPresentableVehicle::AddProjectileLock(int e){if(std::find(projectileLocks.begin(),projectileLocks.end(),e)==projectileLocks.end())projectileLocks.push_back(e);}void idPresentableVehicle::Present(){++presentCount;}void idPresentableVehicle::InventoryAdded(const std::string&i){if(!i.empty())inventory.push_back(i);}void idPresentableVehicle::InitVehiclePhysics(){physics.EnableClip();physicsCallbacks.ActivatePhysics();}void idPresentableVehicle::UpdateCurrentMPH(float s){currentMPH=std::clamp(std::abs(s),0.0F,maxSpeed);}float idPresentableVehicle::GetBoostPower()const{return boostDisabled?0.0F:boostPower;}float idPresentableVehicle::GetBoostDuration()const{return boostDuration;}void idPresentableVehicle::ResetBoostPower(){boostPower=1.0F;}void idPresentableVehicle::BoostForce(float a){if(!boostDisabled&&boostPower>0){boosting=true;currentMPH=std::min(maxSpeed,currentMPH+std::max(0.0F,a));boostPower=std::max(0.0F,boostPower-a*0.01F);}}void idPresentableVehicle::RunPhysics(float dt){if(physics.Evaluate(dt)){++physicsSteps;physics.Translate({currentMPH*dt/3600.0F,0,0});}}void idPresentableVehicle::StopBoost(){boosting=false;}float idPresentableVehicle::GetHealth()const{return health;}bool idPresentableVehicle::GetHasInfiniteAmmo()const{return infiniteAmmo;}void idPresentableVehicle::RemoveProjectileLock(int e){projectileLocks.erase(std::remove(projectileLocks.begin(),projectileLocks.end(),e),projectileLocks.end());}
idVehicleWeapon* idPresentableVehicle::GetWeaponForGroup(int g){return g>=0&&g<static_cast<int>(weaponObjects.size())?&weaponObjects[static_cast<std::size_t>(g)]:nullptr;}float idPresentableVehicle::GetMaxSpeed()const{return maxSpeed;}void idPresentableVehicle::SetBoostDisabled(bool v){boostDisabled=v;if(v)StopBoost();}void idPresentableVehicle::DeleteWeapons(){weaponDecls.clear();weaponAmmo.clear();weaponObjects.clear();currentWeapon=0;}bool idPresentableVehicle::WeaponGroupHasAmmo(int g)const{return infiniteAmmo||(g>=0&&g<static_cast<int>(weaponAmmo.size())&&weaponAmmo[static_cast<std::size_t>(g)]>0);}void idPresentableVehicle::SetCurrentWeapon(int i){if(i>=0&&i<static_cast<int>(weaponDecls.size()))currentWeapon=i;}const std::string& idPresentableVehicle::GetWeaponDecl()const{static const std::string empty;return currentWeapon>=0&&currentWeapon<static_cast<int>(weaponDecls.size())?weaponDecls[static_cast<std::size_t>(currentWeapon)]:empty;}bool idPresentableVehicle::PlayerIsOccupant(int e)const{return std::any_of(occupants.begin(),occupants.end(),[e](const auto&o){return o.entityNumber==e&&o.player;});}vehicleOccupant_t* idPresentableVehicle::GetDriver(){return occupants.empty()?nullptr:&occupants[0];}vehicleOccupant_t* idPresentableVehicle::GetPlayerDriver(){auto*d=GetDriver();return d&&d->player?d:nullptr;}vehicleOccupant_t* idPresentableVehicle::GetGunner(){return occupants.size()>1?&occupants[1]:nullptr;}idEntity* idPresentableVehicle::GetDriverEntity(){auto*d=GetDriver();return d?d->visual:nullptr;}int idPresentableVehicle::GetNumActiveOccupants()const{return static_cast<int>(occupants.size());}bool idPresentableVehicle::IsLocallyControlled()const{return locallyControlled;}bool idPresentableVehicle::IsLocallyControlledByPlayer()const{return locallyControlled&&GetNumActiveOccupants()>0&&occupants[0].player;}bool idPresentableVehicle::IsRemotelyControlledByPlayer()const{return!locallyControlled&&std::any_of(occupants.begin(),occupants.end(),[](const auto&o){return o.player;});}idEntity* idPresentableVehicle::GetLockedTarget()const{return lockedTarget;}
void idPresentableVehicle::SetCurrentWeaponGroup(int g){if(g>=0&&g<static_cast<int>(weaponDecls.size())){currentWeaponGroup=g;currentWeapon=g;}}void idPresentableVehicle::ActivateWeaponGroup(int g){SetCurrentWeaponGroup(g);}void idPresentableVehicle::ActivateCrosshair(){crosshairActive=true;}void idPresentableVehicle::UpdateCrosshair(idEntity*t){lockedTarget=t;crosshair.SetTarget(t);crosshairActive=t!=nullptr;}const std::vector<std::string>& idPresentableVehicle::GetInventory()const{return inventory;}void idPresentableVehicle::Init(){SetDefaults();InitVehiclePhysics();}void idPresentableVehicle::Respawn(){health=100;collisionDamage=0;ResetBoostPower();}void idPresentableVehicle::AddWeaponToVehicle(const std::string&d,int ammo){weaponDecls.push_back(d);weaponAmmo.push_back(std::max(0,ammo));weaponObjects.emplace_back();weaponObjects.back().SetPresentable(this);}void idPresentableVehicle::NextWeapon(){if(!weaponDecls.empty())SetCurrentWeapon((currentWeapon+1)%static_cast<int>(weaponDecls.size()));}void idPresentableVehicle::SetDefaults(){boostDisabled=false;boosting=false;crosshairActive=false;boostDuration=0;boostPower=1;currentMPH=0;maxSpeed=120;health=100;currentWeaponGroup=0;currentWeapon=0;physicsSteps=0;noAmmoCount=0;}
void idPresentableVehicle::Serialize(idSerializer*s){if(!s)return;s->SerializeFloat(boostDuration);s->SerializeFloat(boostPower);s->SerializeFloat(currentMPH);s->SerializeFloat(health);s->SerializeInt(currentWeaponGroup);s->SerializeInt(currentWeapon);s->SerializeBool(boostDisabled);s->SerializeBool(crosshairActive);s->SerializeBool(infiniteAmmo);}void idPresentableVehicle::ClientPredict(float dt){RunPhysics(dt);Interpolate(1);}void idPresentableVehicle::Collide(float impulse){collisionDamage+=std::max(0.0F,impulse);health=std::max(0.0F,health-impulse*0.01F);physicsCallbacks.Contact(impulse);}void idPresentableVehicle::ClientThink(float dt){ClientPredict(dt);UpdateCurrentMPH(currentMPH);}void idPresentableVehicle::ServerThink(float dt){RunPhysics(dt);}
void idPresentableVehicle::GenericOperation(const char* name){++genericRevision;const std::string op=name;const bool emits=op.find("Dust")!=std::string::npos||op.find("Debris")!=std::string::npos||op.find("Throw")!=std::string::npos;if(emits)++debris.emitted;if(op.find("Damage")!=std::string::npos)health=std::max(0.0F,health-1.0F);if(op.find("Update")!=std::string::npos)Present();}
#define WOLF_DEFINE_VEHICLE_GENERIC(name) void idPresentableVehicle::name(){GenericOperation(#name);}
WOLF_VEHICLE_GENERIC_METHODS(WOLF_DEFINE_VEHICLE_GENERIC)
#undef WOLF_DEFINE_VEHICLE_GENERIC

template<typename T>T* idListArrayNew(int count){return count>0?new T[static_cast<std::size_t>(count)]:nullptr;}template<typename T>T* idListArrayResize(T*oldList,int oldCount,int newCount){T*n=idListArrayNew<T>(newCount);for(int i=0;i<std::min(oldCount,newCount);++i)n[i]=oldList[i];delete[]oldList;return n;}template wheelSlipInfo_t*idListArrayNew<wheelSlipInfo_t>(int);template wheelSlipInfo_t*idListArrayResize<wheelSlipInfo_t>(wheelSlipInfo_t*,int,int);
struct idWheel{int index{0};bool operator==(const idWheel&o)const{return index==o.index;}};template class idList<idWheel,5>;template class idList<vehicleOccupant_t,5>;template class idList<idPresentableVehicle::clientDetachable_t,5>;template class idStaticList<idPresentableVehicle::clientDetachable_t,70>;

void idPresentableVehiclePhysicsCB::Collide(float force){Contact(force);}
idPresentableVehicle* idVehicleWeapon::GetPresentable()const{return presentable;}
class idAnimator_VehicleCar{};
template void idPresentableAnimatedEntity::SerializeAnimatorPtr<idAnimator_VehicleCar>(idSerializer*,idAnimator_VehicleCar*&);

idCVar v_simpleCameraMode("v_simpleCameraMode","0",4,"recovered vehicle runtime control");
idCVar v_dusttime("v_dusttime","0",4,"recovered vehicle runtime control");
idCVar v_clientArmor_debug("v_clientArmor_debug","0",4,"recovered vehicle runtime control");
idCVar v_maxTurnSpeed("v_maxTurnSpeed","0",4,"recovered vehicle runtime control");
idCVar v_compressiontime("v_compressiontime","0",4,"recovered vehicle runtime control");
idCVar vehicle_violent_roll("vehicle_violent_roll","0",4,"recovered vehicle runtime control");
idCVar vehicle_violent_boundsscale("vehicle_violent_boundsscale","0",4,"recovered vehicle runtime control");
idCVar vehicle_rcLerp("vehicle_rcLerp","0",4,"recovered vehicle runtime control");
idCVar veh_maxRemoteSimulateSpeed("veh_maxRemoteSimulateSpeed","0",4,"recovered vehicle runtime control");
idCVar veh_maxRemoteSimulateAngSpeed("veh_maxRemoteSimulateAngSpeed","0",4,"recovered vehicle runtime control");
idCVar veh_maxSpeedMP("veh_maxSpeedMP","0",4,"recovered vehicle runtime control");
idCVar veh_maxAngSpeedMP("veh_maxAngSpeedMP","0",4,"recovered vehicle runtime control");
idCVar veh_clientOverrideTime("veh_clientOverrideTime","0",4,"recovered vehicle runtime control");
idCVar veh_useReliableHitMsgs("veh_useReliableHitMsgs","0",4,"recovered vehicle runtime control");
idCVar veh_mpSkidRumbleHighScalar("veh_mpSkidRumbleHighScalar","0",4,"recovered vehicle runtime control");
idCVar veh_mpSkidRumbleLowScalar("veh_mpSkidRumbleLowScalar","0",4,"recovered vehicle runtime control");
idCVar veh_mpSkidRumbleHighDur("veh_mpSkidRumbleHighDur","0",4,"recovered vehicle runtime control");
idCVar veh_mpSkidRumbleLowDur("veh_mpSkidRumbleLowDur","0",4,"recovered vehicle runtime control");
idCVar veh_remoteSimulationThresholdWithNoInfluence("veh_remoteSimulationThresholdWithNoInfluence","0",4,"recovered vehicle runtime control");
idCVar veh_remoteSimulationThreshold("veh_remoteSimulationThreshold","0",4,"recovered vehicle runtime control");
idCVar veh_remoteSimulationAngThresholdWithNoInfluence("veh_remoteSimulationAngThresholdWithNoInfluence","0",4,"recovered vehicle runtime control");
idCVar veh_remoteSimulationAngThreshold("veh_remoteSimulationAngThreshold","0",4,"recovered vehicle runtime control");
idCVar v_testRolloverRoll("v_testRolloverRoll","0",4,"recovered vehicle runtime control");
idCVar v_testRolloverPitch("v_testRolloverPitch","0",4,"recovered vehicle runtime control");
idCVar vehicle_birdsView("vehicle_birdsView","0",4,"recovered vehicle runtime control");
idCVar vehicle_birdsViewDistance("vehicle_birdsViewDistance","0",4,"recovered vehicle runtime control");
idCVar vehicle_passengerFOV("vehicle_passengerFOV","0",4,"recovered vehicle runtime control");
idCVar vehicle_fps_view_lerp_time("vehicle_fps_view_lerp_time","0",4,"recovered vehicle runtime control");
idCVar vehicle_driverview_test("vehicle_driverview_test","0",4,"recovered vehicle runtime control");
idCVar vehicle_fps_stickAngle_scaleAgainstMax("vehicle_fps_stickAngle_scaleAgainstMax","0",4,"recovered vehicle runtime control");
idCVar vehicle_fps_stickAngle_minPitch("vehicle_fps_stickAngle_minPitch","0",4,"recovered vehicle runtime control");
idCVar vehicle_fps_stickAngle_maxPitch("vehicle_fps_stickAngle_maxPitch","0",4,"recovered vehicle runtime control");
idCVar vehicle_fps_stickAngle_minYaw("vehicle_fps_stickAngle_minYaw","0",4,"recovered vehicle runtime control");
idCVar vehicle_fps_stickAngle_maxYaw("vehicle_fps_stickAngle_maxYaw","0",4,"recovered vehicle runtime control");
idCVar vc_wildyaw("vc_wildyaw","0",4,"recovered vehicle runtime control");
idCVar vc_wildpitch("vc_wildpitch","0",4,"recovered vehicle runtime control");
idCVar vc_wildroll("vc_wildroll","0",4,"recovered vehicle runtime control");
idCVar vc_recoverrate("vc_recoverrate","0",4,"recovered vehicle runtime control");
idCVar vc_max_pitch("vc_max_pitch","0",4,"recovered vehicle runtime control");
idCVar v_mouseFreeLook("v_mouseFreeLook","0",4,"recovered vehicle runtime control");
idCVar v_boostSlide("v_boostSlide","0",4,"recovered vehicle runtime control");
idCVar v_jumpSlide("v_jumpSlide","0",4,"recovered vehicle runtime control");
idCVar v_jumpActiveTime("v_jumpActiveTime","0",4,"recovered vehicle runtime control");
idCVar vehicle_remoteRange("vehicle_remoteRange","0",4,"recovered vehicle runtime control");
idCVar v_steerControl("v_steerControl","0",4,"recovered vehicle runtime control");
idCVar v_boostVelocityStabilize("v_boostVelocityStabilize","0",4,"recovered vehicle runtime control");
idCVar v_steerControlBoostRate("v_steerControlBoostRate","0",4,"recovered vehicle runtime control");
idCVar v_steerControlRate("v_steerControlRate","0",4,"recovered vehicle runtime control");
idCVar v_steerControlTargetSpeed("v_steerControlTargetSpeed","0",4,"recovered vehicle runtime control");
idCVar v_steerControlInfluence("v_steerControlInfluence","0",4,"recovered vehicle runtime control");
idCVar v_steerControlClamp("v_steerControlClamp","0",4,"recovered vehicle runtime control");
idCVar v_rollStabilization("v_rollStabilization","0",4,"recovered vehicle runtime control");
idCVar v_pitchStabilization("v_pitchStabilization","0",4,"recovered vehicle runtime control");
idCVar v_skidRoll("v_skidRoll","0",4,"recovered vehicle runtime control");
idCVar v_spinoutAngle("v_spinoutAngle","0",4,"recovered vehicle runtime control");
idCVar v_handbrake_cam_lerp_enable("v_handbrake_cam_lerp_enable","0",4,"recovered vehicle runtime control");
idCVar v_handbrake_cam_lerp("v_handbrake_cam_lerp","0",4,"recovered vehicle runtime control");
idCVar v_handbrake_cam_lerp_power("v_handbrake_cam_lerp_power","0",4,"recovered vehicle runtime control");
idCVar v_handbrake_cam_lerp_min_mult("v_handbrake_cam_lerp_min_mult","0",4,"recovered vehicle runtime control");
idCVar v_handbrake_skid_lerp_enable("v_handbrake_skid_lerp_enable","0",4,"recovered vehicle runtime control");
idCVar v_handbrake_skid_lerp("v_handbrake_skid_lerp","0",4,"recovered vehicle runtime control");
idCVar v_handbrake_skid_lerp_power("v_handbrake_skid_lerp_power","0",4,"recovered vehicle runtime control");
idCVar v_handbrake_gripout_lerp_enable("v_handbrake_gripout_lerp_enable","0",4,"recovered vehicle runtime control");
idCVar v_handbrake_gripout_lerp("v_handbrake_gripout_lerp","0",4,"recovered vehicle runtime control");
idCVar v_handbrake_gripout_lerp_power("v_handbrake_gripout_lerp_power","0",4,"recovered vehicle runtime control");
idCVar v_handbrake_gripin_lerp_enable("v_handbrake_gripin_lerp_enable","0",4,"recovered vehicle runtime control");
idCVar v_handbrake_gripin_lerp("v_handbrake_gripin_lerp","0",4,"recovered vehicle runtime control");
idCVar v_handbrake_gripin_lerp_power("v_handbrake_gripin_lerp_power","0",4,"recovered vehicle runtime control");
idCVar vehicle_baseFOV("vehicle_baseFOV","0",4,"recovered vehicle runtime control");
idCVar vehicle_maxBaseFOV("vehicle_maxBaseFOV","0",4,"recovered vehicle runtime control");
idCVar v_mpQuickUseOnSelect("v_mpQuickUseOnSelect","0",4,"recovered vehicle runtime control");
idCVar veh_eval("veh_eval","0",4,"recovered vehicle runtime control");
idCVar veh_debug_print("veh_debug_print","0",4,"recovered vehicle runtime control");
idCVar veh_volume_brake_whine("veh_volume_brake_whine","0",4,"recovered vehicle runtime control");
idCVar veh_volume_shift_up("veh_volume_shift_up","0",4,"recovered vehicle runtime control");
idCVar veh_volume_shift_down("veh_volume_shift_down","0",4,"recovered vehicle runtime control");
idCVar veh_visual_dust_scale_min("veh_visual_dust_scale_min","0",4,"recovered vehicle runtime control");
idCVar vehicle_cam_new("vehicle_cam_new","0",4,"recovered vehicle runtime control");
idCVar vehicle_blurOffset("vehicle_blurOffset","0",4,"recovered vehicle runtime control");
idCVar vehicle_blurValue("vehicle_blurValue","0",4,"recovered vehicle runtime control");
idCVar vehicle_blurMaxScale("vehicle_blurMaxScale","0",4,"recovered vehicle runtime control");
idCVar vehicle_blurDuration("vehicle_blurDuration","0",4,"recovered vehicle runtime control");
idCVar vehicle_blurRampUp("vehicle_blurRampUp","0",4,"recovered vehicle runtime control");
idCVar vehicle_blurRampDown("vehicle_blurRampDown","0",4,"recovered vehicle runtime control");
idCVar veh_simulateRemoteVehicles("veh_simulateRemoteVehicles","0",4,"recovered vehicle runtime control");
idCVar veh_impulseScaleMP("veh_impulseScaleMP","0",4,"recovered vehicle runtime control");
idCVar veh_hitControlDelay("veh_hitControlDelay","0",4,"recovered vehicle runtime control");
idCVar vehicle_swerveForce("vehicle_swerveForce","0",4,"recovered vehicle runtime control");
idCVar vehicle_stuckUpForce("vehicle_stuckUpForce","0",4,"recovered vehicle runtime control");
idCVar vehicle_stuckLateralForce("vehicle_stuckLateralForce","0",4,"recovered vehicle runtime control");
idCVar vehicle_extra_headon_impulse("vehicle_extra_headon_impulse","0",4,"recovered vehicle runtime control");
idCVar vehicle_extra_headon_impulse_mult("vehicle_extra_headon_impulse_mult","0",4,"recovered vehicle runtime control");
idCVar vehicle_extra_headon_impulse_offset("vehicle_extra_headon_impulse_offset","0",4,"recovered vehicle runtime control");
idCVar vehicle_extra_headon_impulse_mult_rc("vehicle_extra_headon_impulse_mult_rc","0",4,"recovered vehicle runtime control");
idCVar vehicle_extra_headon_impulse_offset_rc("vehicle_extra_headon_impulse_offset_rc","0",4,"recovered vehicle runtime control");
idCVar vehicle_extra_headon_impulse_debug("vehicle_extra_headon_impulse_debug","0",4,"recovered vehicle runtime control");
idCVar vehicle_fx_engineSmokeThreshold("vehicle_fx_engineSmokeThreshold","0",4,"recovered vehicle runtime control");
idCVar vehicle_clientImpactDmg_MinVel("vehicle_clientImpactDmg_MinVel","0",4,"recovered vehicle runtime control");
idCVar vehicle_clientImpactDmg_MaxVel("vehicle_clientImpactDmg_MaxVel","0",4,"recovered vehicle runtime control");
idCVar vehicle_clientImpactDmg_MinDmg("vehicle_clientImpactDmg_MinDmg","0",4,"recovered vehicle runtime control");
idCVar vehicle_clientImpactDmg_MaxDmg("vehicle_clientImpactDmg_MaxDmg","0",4,"recovered vehicle runtime control");
idCVar veh_damage_fx_level1_start("veh_damage_fx_level1_start","0",4,"recovered vehicle runtime control");
idCVar veh_damage_fx_level2_start("veh_damage_fx_level2_start","0",4,"recovered vehicle runtime control");
idCVar veh_damage_fx_level3_start("veh_damage_fx_level3_start","0",4,"recovered vehicle runtime control");
idCVar veh_damage_fx_level4_start("veh_damage_fx_level4_start","0",4,"recovered vehicle runtime control");
idCVar veh_damage_fx_level1_start_mp("veh_damage_fx_level1_start_mp","0",4,"recovered vehicle runtime control");
idCVar veh_damage_fx_level2_start_mp("veh_damage_fx_level2_start_mp","0",4,"recovered vehicle runtime control");
idCVar veh_damage_fx_level3_start_mp("veh_damage_fx_level3_start_mp","0",4,"recovered vehicle runtime control");
idCVar veh_damage_fx_level4_start_mp("veh_damage_fx_level4_start_mp","0",4,"recovered vehicle runtime control");
idCVar vehicle_mp_defOrigin("vehicle_mp_defOrigin","0",4,"recovered vehicle runtime control");
idCVar vehicle_health_debug("vehicle_health_debug","0",4,"recovered vehicle runtime control");
idCVar vehicle_soundImpactForce("vehicle_soundImpactForce","0",4,"recovered vehicle runtime control");
idCVar vehicle_online_boostExtenderCooldown("vehicle_online_boostExtenderCooldown","0",4,"recovered vehicle runtime control");
idCVar vehicle_showWheels("vehicle_showWheels","0",4,"recovered vehicle runtime control");
idCVar vehicle_online_skipServerPhysicsCB("vehicle_online_skipServerPhysicsCB","0",4,"recovered vehicle runtime control");
idCVar vehicle_showExhaust("vehicle_showExhaust","0",4,"recovered vehicle runtime control");
idCVar vehicle_useCompressionPitch("vehicle_useCompressionPitch","0",4,"recovered vehicle runtime control");
idCVar vehicle_showCompressionPitch("vehicle_showCompressionPitch","0",4,"recovered vehicle runtime control");
idCVar vehicle_compressionPitchMin("vehicle_compressionPitchMin","0",4,"recovered vehicle runtime control");
idCVar vehicle_compressionPitchMax("vehicle_compressionPitchMax","0",4,"recovered vehicle runtime control");
idCVar vehicle_compressionPitchMinScale("vehicle_compressionPitchMinScale","0",4,"recovered vehicle runtime control");
idCVar vehicle_compressionPitchMaxScale("vehicle_compressionPitchMaxScale","0",4,"recovered vehicle runtime control");
idCVar vehicle_maxGear("vehicle_maxGear","0",4,"recovered vehicle runtime control");
idCVar vehicle_sound_skipBody("vehicle_sound_skipBody","0",4,"recovered vehicle runtime control");
idCVar vehicle_trailMaxDist("vehicle_trailMaxDist","0",4,"recovered vehicle runtime control");
idCVar g_tireTrackLengthScaleSpeed("g_tireTrackLengthScaleSpeed","0",4,"recovered vehicle runtime control");
idCVar g_tireTrackLengthScale("g_tireTrackLengthScale","0",4,"recovered vehicle runtime control");
idCVar vehicle_reverseTorqueMultiplier("vehicle_reverseTorqueMultiplier","0",4,"recovered vehicle runtime control");
idCVar vehicle_emp_mode("vehicle_emp_mode","0",4,"recovered vehicle runtime control");
idCVar vehicle_emp_handbrake("vehicle_emp_handbrake","0",4,"recovered vehicle runtime control");
idCVar vehicle_emp_brakeForce("vehicle_emp_brakeForce","0",4,"recovered vehicle runtime control");
idCVar vehicle_jumpButton_verticalImpulse("vehicle_jumpButton_verticalImpulse","0",4,"recovered vehicle runtime control");
idCVar v_airControlDelay("v_airControlDelay","0",4,"recovered vehicle runtime control");
idCVar v_airControlLowerThreshold("v_airControlLowerThreshold","0",4,"recovered vehicle runtime control");
idCVar v_airControlUpperThreshold("v_airControlUpperThreshold","0",4,"recovered vehicle runtime control");
idCVar v_printAirControlDelay("v_printAirControlDelay","0",4,"recovered vehicle runtime control");
idCVar weapon_overheatEnable("weapon_overheatEnable","0",4,"recovered vehicle runtime control");
idCVar weapon_overheatCoolPercentage("weapon_overheatCoolPercentage","0",4,"recovered vehicle runtime control");
idCVar weapon_overheatCooldownScale("weapon_overheatCooldownScale","0",4,"recovered vehicle runtime control");
idCVar weapon_overheatDebug("weapon_overheatDebug","0",4,"recovered vehicle runtime control");
idCVar mp_vehicle_DamageDebug("mp_vehicle_DamageDebug","0",4,"recovered vehicle runtime control");
idCVar vehicle_wview_lag("vehicle_wview_lag","0",4,"recovered vehicle runtime control");
idCVar vehicle_wview_lag_clamp("vehicle_wview_lag_clamp","0",4,"recovered vehicle runtime control");
idCVar vehicle_wview_alag("vehicle_wview_alag","0",4,"recovered vehicle runtime control");
idCVar vehicle_wview_fwd("vehicle_wview_fwd","0",4,"recovered vehicle runtime control");
idCVar vehicle_wview_rt("vehicle_wview_rt","0",4,"recovered vehicle runtime control");
idCVar vehicle_wview_up("vehicle_wview_up","0",4,"recovered vehicle runtime control");
idCVar vehicle_whiplash_min_frac("vehicle_whiplash_min_frac","0",4,"recovered vehicle runtime control");
idCVar vehicle_whiplash_max_ang("vehicle_whiplash_max_ang","0",4,"recovered vehicle runtime control");
idCVar vehicle_whiplash_exp("vehicle_whiplash_exp","0",4,"recovered vehicle runtime control");
idCVar vehicle_whiplash_angvel_max_ang("vehicle_whiplash_angvel_max_ang","0",4,"recovered vehicle runtime control");
idCVar vehicle_whiplash_angvel_min_frac("vehicle_whiplash_angvel_min_frac","0",4,"recovered vehicle runtime control");
idCVar vehicle_whiplash_angvel_high("vehicle_whiplash_angvel_high","0",4,"recovered vehicle runtime control");
idCVar vehicle_whiplash_angvel_exp("vehicle_whiplash_angvel_exp","0",4,"recovered vehicle runtime control");
