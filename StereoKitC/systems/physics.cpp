#pragma comment(lib, "reactphysics3d.lib")

#include "physics.h"
#include "../stereokit.h"
#include "../_stereokit.h"

#include <vector>
using namespace std;
#include <reactphysics3d.h>
using namespace reactphysics3d;

namespace sk {

///////////////////////////////////////////

struct solid_move_t {
	RigidBody *body;
	Vector3 dest;
	Quaternion dest_rot;
	Vector3 old_velocity;
	Vector3 old_rot_velocity;
};
vector<solid_move_t> solid_moves;

struct physics_shape_asset_t {
	int64_t id;
	void   *shape;
	int     refs;
};

double physics_sim_time = 0;
double physics_step = 1 / 90.0;

DynamicsWorld *physics_world;

vector<physics_shape_asset_t> physics_shapes;

///////////////////////////////////////////

bool physics_init() {

	WorldSettings settings;
	physics_world = new DynamicsWorld(Vector3(0,-9.81f,0), settings);
	return true;
}

///////////////////////////////////////////

void physics_shutdown() {
	delete physics_world;
}

///////////////////////////////////////////

void physics_update() {
	// How many physics frames are we going to be calculating this time?
	int32_t frames = (int32_t)ceil((sk_timev - physics_sim_time) / physics_step);
	if (frames <= 0)
		return;

	// Calculate move velocities for objects that need to be at their destination by the end of this function!
	for (size_t i = 0; i < solid_moves.size(); i++) {
		solid_move_t &move = solid_moves[i];
		// Position
		move.old_velocity  = move.body->getLinearVelocity();
		Vector3       pos  = move.body->getTransform().getPosition();
		Vector3       velocity = (move.dest - pos) / (reactphysics3d::decimal)(physics_step * frames);
		move.body->setLinearVelocity(velocity);
		// Rotation
		move.old_rot_velocity = move.body->getAngularVelocity();
		Quaternion rot   = move.body->getTransform().getOrientation();
		if (rot.x != move.dest_rot.x || rot.y != move.dest_rot.y || rot.z != move.dest_rot.z || rot.w != move.dest_rot.w) {
			Quaternion delta = move.dest_rot * rot.getInverse();
			float   angle;
			Vector3 axis;
			delta.getRotationAngleAxis(angle, axis);
			if (!isnan(angle)) {
				move.body->setAngularVelocity((angle / (reactphysics3d::decimal)(physics_step * frames)) * axis.getUnit());
			}
		}
	}

	// Sim physics!
	while (physics_sim_time < sk_timev) {
		physics_world->update((reactphysics3d::decimal)physics_step);
		physics_sim_time += physics_step;
	}

	// Reset moved objects back to their original values, and clear out our list
	for (size_t i = 0; i < solid_moves.size(); i++) {
		solid_moves[i].body->setLinearVelocity (solid_moves[i].old_velocity);
		solid_moves[i].body->setAngularVelocity(solid_moves[i].old_rot_velocity);
	}
	solid_moves.clear();
}

///////////////////////////////////////////

solid_t solid_create(const vec3 &position, const quat &rotation, solid_type_ type) {
	RigidBody *body = physics_world->createRigidBody(Transform((Vector3 &)position, (Quaternion &)rotation));
	solid_set_type(body, type);
	return (solid_t)body;
}

///////////////////////////////////////////

void solid_release(solid_t solid) {
	if (solid == nullptr)
		return;

	RigidBody        *body  = (RigidBody*)solid;
	const ProxyShape *shape = body->getProxyShapesList();
	while (shape != nullptr) {
		const CollisionShape *asset = shape->getCollisionShape();

		CollisionShapeName name = asset->getName();
		if (name == CollisionShapeName::BOX || name == CollisionShapeName::SPHERE || name == CollisionShapeName::CAPSULE)
			delete asset;
		else
			log_warn("Haven't added support for all physics shapes yet!");

		shape = shape->getNext();
	}

	physics_world->destroyRigidBody((RigidBody *)solid);
}

///////////////////////////////////////////

void solid_add_sphere(solid_t solid, float diameter, float kilograms, const vec3 *offset) {
	RigidBody   *body   = (RigidBody*)solid;
	SphereShape *sphere = new SphereShape(diameter/2);
	body->addCollisionShape(sphere, Transform(offset == nullptr ? Vector3(0,0,0) : (Vector3 &)*offset, { 0,0,0,1 }), kilograms);
}

///////////////////////////////////////////

void solid_add_box(solid_t solid, const vec3 &dimensions, float kilograms, const vec3 *offset) {
	RigidBody *body = (RigidBody*)solid;
	BoxShape  *box  = new BoxShape((Vector3&)(dimensions/2));
	body->addCollisionShape(box, Transform(offset == nullptr ? Vector3(0,0,0) : (Vector3 &)*offset, { 0,0,0,1 }), kilograms);
}

///////////////////////////////////////////

void solid_add_capsule(solid_t solid, float diameter, float height, float kilograms, const vec3 *offset) {
	RigidBody    *body    = (RigidBody*)solid;
	CapsuleShape *capsule = new CapsuleShape(diameter/2, height);
	body->addCollisionShape(capsule, Transform(offset == nullptr ? Vector3(0,0,0) : (Vector3 &)*offset, { 0,0,0,1 }), kilograms);
}

///////////////////////////////////////////

void solid_set_type(solid_t solid, solid_type_ type) {
	RigidBody *body = (RigidBody *)solid;

	switch (type) {
	case solid_type_normal:     body->setType(BodyType::DYNAMIC);   break;
	case solid_type_immovable:  body->setType(BodyType::STATIC);    break;
	case solid_type_unaffected: body->setType(BodyType::KINEMATIC); break;
	}
}

///////////////////////////////////////////

void solid_set_enabled(solid_t solid, bool32_t enabled) {
	RigidBody *body = (RigidBody *)solid;
	body->setIsActive(enabled);
}

///////////////////////////////////////////

void solid_teleport(solid_t solid, const vec3 &position, const quat &rotation) {
	RigidBody *body = (RigidBody *)solid;Transform t = Transform((Vector3 &)position, (Quaternion &)rotation);
	body->setTransform(t);
	body->setIsSleeping(false);
}

///////////////////////////////////////////

void solid_move(solid_t solid, const vec3 &position, const quat &rotation) {
	RigidBody *body = (RigidBody *)solid;
	solid_moves.push_back(solid_move_t{body, Vector3(position.x, position.y, position.z), Quaternion(rotation.i, rotation.j, rotation.k, rotation.a)});
}

///////////////////////////////////////////

void solid_set_velocity(solid_t solid, const vec3 &meters_per_second) {
	RigidBody *body = (RigidBody *)solid;
	body->setLinearVelocity((Vector3&)meters_per_second);
}

///////////////////////////////////////////

void solid_set_velocity_ang(solid_t solid, const vec3 &radians_per_second) {
	RigidBody *body = (RigidBody *)solid;
	body->setAngularVelocity((Vector3&)radians_per_second);
}

///////////////////////////////////////////

void solid_get_transform(const solid_t solid, transform_t &out_transform) {
	const Transform &solid_tr = ((RigidBody *)solid)->getTransform();
	memcpy(&out_transform._position, &solid_tr.getPosition   ().x, sizeof(vec3));
	memcpy(&out_transform._rotation, &solid_tr.getOrientation().x, sizeof(quat));
	out_transform._dirty = true;
}

} // namespace sk