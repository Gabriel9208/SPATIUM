//
// Created by gabriel on 4/12/26.
//

#ifndef SPATIUM_IGPURESOURCE_H
#define SPATIUM_IGPURESOURCE_H



class IGPUResource {
public:
    virtual ~IGPUResource() = default;
    virtual void bind() const = 0;
    virtual void unbind() const = 0;
};



#endif //SPATIUM_IGPURESOURCE_H
