"""
Compatibility module for handling deprecated Unreal Engine Python API calls.
Provides wrapper functions that use the newer recommended APIs where available.
"""

import unreal

def get_editor_world():
    """
    Get the current editor world using the recommended API.
    Falls back to deprecated method if new API is not available.
    """
    try:
        # Try new recommended API
        if hasattr(unreal, 'UnrealEditorSubsystem'):
            subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
            if hasattr(subsystem, 'get_editor_world'):
                return subsystem.get_editor_world()
    except:
        pass
    
    # Fallback to deprecated but working method
    if hasattr(unreal, 'EditorLevelLibrary'):
        return unreal.EditorLevelLibrary.get_editor_world()
    
    return None

def get_all_level_actors():
    """
    Get all actors in the current level using the recommended API.
    Falls back to deprecated method if new API is not available.
    """
    try:
        # Try new recommended API - EditorActorSubsystem
        if hasattr(unreal, 'EditorActorSubsystem'):
            subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
            if hasattr(subsystem, 'get_all_level_actors'):
                return subsystem.get_all_level_actors()
    except:
        pass
    
    # Fallback to deprecated but working method
    if hasattr(unreal, 'EditorLevelLibrary'):
        return unreal.EditorLevelLibrary.get_all_level_actors()
    
    return []

def spawn_actor_from_class(actor_class, location, rotation):
    """
    Spawn an actor in the level using the recommended API.
    Falls back to deprecated method if new API is not available.
    """
    try:
        # Try new recommended API
        if hasattr(unreal, 'EditorActorSubsystem'):
            subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
            if hasattr(subsystem, 'spawn_actor_from_class'):
                return subsystem.spawn_actor_from_class(actor_class, location, rotation)
    except:
        pass
    
    # Fallback to deprecated but working method
    if hasattr(unreal, 'EditorLevelLibrary'):
        return unreal.EditorLevelLibrary.spawn_actor_from_class(actor_class, location, rotation)
    
    return None

def destroy_actor(actor):
    """
    Destroy an actor in the level using the recommended API.
    Falls back to deprecated method if new API is not available.
    """
    try:
        # Try new recommended API
        if hasattr(unreal, 'EditorActorSubsystem'):
            subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
            if hasattr(subsystem, 'destroy_actor'):
                return subsystem.destroy_actor(actor)
    except:
        pass
    
    # Fallback to deprecated but working method
    if hasattr(unreal, 'EditorLevelLibrary'):
        return unreal.EditorLevelLibrary.destroy_actor(actor)
    
    return False

def save_current_level():
    """
    Save the current level using the recommended API.
    Falls back to deprecated method if new API is not available.
    """
    try:
        # Try new recommended API
        if hasattr(unreal, 'UnrealEditorSubsystem'):
            subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
            if hasattr(subsystem, 'save_current_level'):
                return subsystem.save_current_level()
    except:
        pass
    
    # Fallback to deprecated but working method
    if hasattr(unreal, 'EditorLevelLibrary'):
        return unreal.EditorLevelLibrary.save_current_level()
    
    return False

def get_level_viewport_camera_info():
    """
    Get level viewport camera information using the recommended API.
    Falls back to deprecated method if new API is not available.
    """
    try:
        # Try new recommended API
        if hasattr(unreal, 'UnrealEditorSubsystem'):
            subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
            if hasattr(subsystem, 'get_level_viewport_camera_info'):
                return subsystem.get_level_viewport_camera_info()
    except:
        pass
    
    # Fallback to deprecated but working method
    if hasattr(unreal, 'EditorLevelLibrary'):
        return unreal.EditorLevelLibrary.get_level_viewport_camera_info()
    
    return None

# Blueprint compatibility functions
def get_blueprint_generated_class(blueprint):
    """
    Get the generated class from a blueprint using the recommended API.
    """
    try:
        # Try using BlueprintEditorLibrary
        if hasattr(unreal, 'BlueprintEditorLibrary'):
            return unreal.BlueprintEditorLibrary.generated_class(blueprint)
    except:
        pass
    
    # Try getting it as a property (might not work)
    try:
        return blueprint.get_editor_property('generated_class')
    except:
        pass
    
    return None

def get_blueprint_parent_class(blueprint):
    """
    Get the parent class from a blueprint.
    """
    try:
        # Try getting it as a property
        return blueprint.get_editor_property('parent_class')
    except:
        pass
    
    # Default to Actor for most blueprints
    return unreal.Actor if hasattr(unreal, 'Actor') else None

def get_class_name(unreal_class):
    """
    Get the name of an Unreal class object safely.
    """
    if not unreal_class:
        return "None"
    
    # Try various methods to get the class name
    try:
        if hasattr(unreal_class, '__name__'):
            return unreal_class.__name__
    except:
        pass
    
    try:
        if hasattr(unreal_class, 'get_name'):
            # get_name() is a method on instances, not classes
            # So we need to be careful here
            return str(unreal_class).split('.')[-1].replace("'", "").replace('>', '')
    except:
        pass
    
    # Fallback to string representation
    class_str = str(unreal_class)
    if '.' in class_str:
        return class_str.split('.')[-1].replace("'", "").replace('>', '')
    
    return class_str

def ensure_kismet_system_library():
    """
    Try to import KismetSystemLibrary if available.
    Note: This library might not be available in all UE versions or configurations.
    """
    try:
        # KismetSystemLibrary is part of the BlueprintGraph module
        # It might not be exposed to Python in all versions
        if hasattr(unreal, 'KismetSystemLibrary'):
            return unreal.KismetSystemLibrary
        
        # Try alternate import methods
        import importlib
        try:
            kismet = importlib.import_module('unreal.KismetSystemLibrary')
            return kismet
        except:
            pass
    except:
        pass
    
    return None
